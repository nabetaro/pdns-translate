/*
    PowerDNS Versatile Database Driven Nameserver
    Copyright (C) 2002 - 2009 PowerDNS.COM BV

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License version 2 as 
    published by the Free Software Foundation.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*/
#include "packetcache.hh"
#include "utility.hh"
#include "resolver.hh"
#include <pthread.h>
#include <semaphore.h>
#include <iostream>
#include <errno.h>
#include "misc.hh"
#include <algorithm>
#include <sstream>
#include "dnsrecords.hh"
#include <cstring>
#include <string>
#include <vector>
#include <boost/algorithm/string.hpp>
#include "dns.hh"
#include "qtype.hh"
#include "tcpreceiver.hh"
#include "ahuexception.hh"
#include "statbag.hh"
#include "arguments.hh"
#include "dnswriter.hh"
#include "dnsparser.hh"
#include <boost/shared_ptr.hpp>
#include <boost/foreach.hpp>
#include "dns_random.hh"

#include "namespaces.hh"

void Resolver::makeUDPSocket()
{
  makeSocket(SOCK_DGRAM);
}

void Resolver::makeSocket(int type)
{
  static uint16_t port_counter=5000;
  port_counter++; // this makes us use a new port for each query, fixes ticket #2
  if(d_sock>0)
    return;

  d_sock=socket(AF_INET, type,0);
  if(d_sock<0) 
    throw AhuException("Making a socket for resolver: "+stringerror());

  struct sockaddr_in sin;
  memset((char *)&sin,0, sizeof(sin));
  
  sin.sin_family = AF_INET;
  if(!IpToU32(::arg()["query-local-address"], &sin.sin_addr.s_addr))
    throw AhuException("Unable to resolve local address '"+ ::arg()["query-local-address"] +"'"); 

  int tries=10;
  while(--tries) {
    sin.sin_port = htons(10000+(dns_random(10000)));
  
    if (::bind(d_sock, (struct sockaddr *)&sin, sizeof(sin)) >= 0) 
      break;

  }
  if(!tries) {
    Utility::closesocket(d_sock);
    d_sock=-1;
    throw AhuException("Resolver binding to local socket: "+stringerror());
  }
}

Resolver::Resolver()
{
  d_sock=-1;
  d_timeout=500000;
  d_soacount=0;
  d_buf=new unsigned char[66000];
}

Resolver::~Resolver()
{
  if(d_sock>=0)
    Utility::closesocket(d_sock);
  delete[] d_buf;
}

void Resolver::timeoutReadn(char *buffer, int bytes)
{
  time_t start=time(0);
  int n=0;
  int numread;
  while(n<bytes) {
    if(waitForData(d_sock, 10-(time(0)-start))<0)
      throw ResolverException("Reading data from remote nameserver over TCP: "+stringerror());

    numread=recv(d_sock,buffer+n,bytes-n,0);
    if(numread<0)
      throw ResolverException("Reading data from remote nameserver over TCP: "+stringerror());
    if(numread==0)
      throw ResolverException("Remote nameserver closed TCP connection");
    n+=numread;
  }
}

int Resolver::notify(int sock, const string &domain, const string &ip, uint16_t id)
{
  vector<uint8_t> packet;
  DNSPacketWriter pw(packet, domain, QType::SOA, 1, Opcode::Notify);
  pw.getHeader()->id = d_randomid = id;
  pw.getHeader()->aa = true; 
  
  ComboAddress dest(ip, 53);

  if(sendto(sock, &packet[0], packet.size(), 0, (struct sockaddr*)(&dest), dest.getSocklen())<0) {
    throw ResolverException("Unable to send notify to "+ip+": "+stringerror());
  }
  return true;
}

uint16_t Resolver::sendResolve(const string &ip, const char *domain, int type, bool dnssecOK)
{
  vector<uint8_t> packet;
  DNSPacketWriter pw(packet, domain, type);
  pw.getHeader()->id = d_randomid = dns_random(0xffff);

  if(dnssecOK) {
    pw.addOpt(2800, 0, EDNSOpts::DNSSECOK);
    pw.commit();
  }

  d_domain=domain;
  d_type=type;
  d_inaxfr=false;

  ServiceTuple st;
  st.port=53;
  try {
    parseService(ip, st);
  }
  catch(AhuException &ae) {
    throw ResolverException("Sending a dns question to '"+ip+"': "+ae.reason);
  }

  ComboAddress remote(st.host, st.port);

  if(sendto(d_sock, &packet[0], packet.size(), 0, (struct sockaddr*)(&remote), remote.getSocklen()) < 0) {
    throw ResolverException("Unable to ask query of "+st.host+":"+itoa(st.port)+": "+stringerror());
  }
  return d_randomid;
}

bool Resolver::tryGetSOASerial(string* domain, uint32_t *theirSerial, uint32_t *theirInception, uint32_t *theirExpire, uint16_t* id)
{
  Utility::setNonBlocking( d_sock );
  
  if(waitForData(d_sock, 0, 500000) == 0)
    return false;
  
  int err;
  ComboAddress fromaddr;
  socklen_t addrlen=fromaddr.getSocklen();
  err = recvfrom(d_sock, reinterpret_cast< char * >( d_buf ), 512, 0,(struct sockaddr*)(&fromaddr), &addrlen);
  if(err < 0) {
    if(errno == EAGAIN)
      return false;
    
    throw ResolverException("recvfrom error waiting for answer: "+stringerror());
  }
  
  MOADNSParser mdp((char*)d_buf, err);
  *id=mdp.d_header.id;
  *domain = stripDot(mdp.d_qname);
  
  if(mdp.d_answers.empty())
    throw ResolverException("Query to '" + fromaddr.toString() + "' for SOA of '" + *domain + "' produced no results");
  
  if(mdp.d_qtype != QType::SOA)
    throw ResolverException("Query to '" + fromaddr.toString() + "' for SOA of '" + *domain + "' returned wrong record type");

  *theirInception = *theirExpire = 0;
  bool gotSOA=false;
  BOOST_FOREACH(const MOADNSParser::answers_t::value_type& drc, mdp.d_answers) {
    if(drc.first.d_type == QType::SOA) {
      shared_ptr<SOARecordContent> src=boost::dynamic_pointer_cast<SOARecordContent>(drc.first.d_content);
      *theirSerial=src->d_st.serial;
      gotSOA = true;
    }
    if(drc.first.d_type == QType::RRSIG) {
      shared_ptr<RRSIGRecordContent> rrc=boost::dynamic_pointer_cast<RRSIGRecordContent>(drc.first.d_content);
      *theirInception= std::max(*theirInception, rrc->d_siginception);
      *theirExpire = std::max(*theirExpire, rrc->d_sigexpire);
    }
  }
  if(!gotSOA)
    throw ResolverException("Query to '" + fromaddr.toString() + "' for SOA of '" + *domain + "' did not return a SOA");
  return true;
}

int Resolver::receiveResolve(struct sockaddr* fromaddr, Utility::socklen_t addrlen)
{
  int res=waitForData(d_sock, 0, 7500000); 
  
  if(!res) {
    throw ResolverException("Timeout waiting for answer");
  }
  if(res<0)
    throw ResolverException("Error waiting for answer: "+stringerror());


  if((d_len=recvfrom(d_sock, reinterpret_cast< char * >( d_buf ), 512,0,(struct sockaddr*)(fromaddr), &addrlen))<0) 
    throw ResolverException("recvfrom error waiting for answer: "+stringerror());

  return 1;
}
  
int Resolver::resolve(const string &ipport, const char *domain, int type)
{
  makeUDPSocket();
  sendResolve(ipport, domain, type);
  try {
    ServiceTuple st;
    st.port = 53;   
    parseService(ipport, st);
    ComboAddress from(st.host, st.port);

    return receiveResolve((sockaddr*)&from, from.getSocklen());
  }
  catch(ResolverException &re) {
    throw ResolverException(re.reason+" from "+ipport);
  }
  return 1;
  
}

void Resolver::makeTCPSocket(const string &ip, uint16_t port)
{
  if(d_sock>=0)
    return;

  d_toaddr=ComboAddress(ip, port);

  d_sock=socket(AF_INET,SOCK_STREAM,0);
  if(d_sock<0)
    throw ResolverException("Unable to make a TCP socket for resolver: "+stringerror());

  // Use query-local-address as source IP for queries, if specified.
  string querylocaladdress(::arg()["query-local-address"]);
  if (!querylocaladdress.empty()) {
    ComboAddress fromaddr(querylocaladdress, 0);

    if (::bind(d_sock, (struct sockaddr *)&fromaddr, fromaddr.getSocklen()) < 0) {
      Utility::closesocket(d_sock);
      d_sock=-1;
      throw ResolverException("Binding to query-local-address: "+stringerror());
    }
  }  
    
  Utility::setNonBlocking( d_sock );

  int err;
#ifndef WIN32
  if((err=connect(d_sock,(struct sockaddr*)&d_toaddr, d_toaddr.getSocklen()))<0 && errno!=EINPROGRESS) {
#else
  if((err=connect(d_sock,(struct sockaddr*)&d_toaddr, d_toaddr.getSocklen()))<0 && WSAGetLastError() != WSAEWOULDBLOCK ) {
#endif // WIN32
    Utility::closesocket(d_sock);
    d_sock=-1;
    throw ResolverException("connect: "+stringerror());
  }

  if(!err)
    goto done;

  err=waitForRWData(d_sock, false, 10, 0); // wait for writeability
  
  if(!err) {
    Utility::closesocket(d_sock); // timeout
    d_sock=-1;
    errno=ETIMEDOUT;
    
    throw ResolverException("Timeout connecting to server");
  }
  else if(err < 0) {
    throw ResolverException("Error connecting: "+string(strerror(err)));
  }
  else {
    Utility::socklen_t len=sizeof(err);
    if(getsockopt(d_sock, SOL_SOCKET,SO_ERROR,(char *)&err,&len)<0)
      throw ResolverException("Error connecting: "+stringerror()); // Solaris

    if(err)
      throw ResolverException("Error connecting: "+string(strerror(err)));
  }
  
 done:
  Utility::setBlocking( d_sock );
  // d_sock now connected
}

//! returns -1 for permanent error, 0 for timeout, 1 for success
int Resolver::axfr(const string &ipport, const char *domain)
{
  d_domain=domain;

  
  ServiceTuple st;  
  st.port = 53;     
  parseService(ipport, st);
  makeTCPSocket(st.host, st.port);

  d_type=QType::AXFR;
  
  vector<uint8_t> packet;
  DNSPacketWriter pw(packet, domain, QType::AXFR);
  pw.getHeader()->id = d_randomid = dns_random(0xffff);

  uint16_t replen=htons(packet.size());
  Utility::iovec iov[2];
  iov[0].iov_base=(char*)&replen;
  iov[0].iov_len=2;
  iov[1].iov_base=(char*)&packet[0];
  iov[1].iov_len=packet.size();

  int ret=Utility::writev(d_sock,iov,2);
  if(ret<0)
    throw ResolverException("Error sending question to "+ipport+": "+stringerror());

  int res = waitForData(d_sock, 10, 0);
  
  if(!res)
    throw ResolverException("Timeout waiting for answer from "+ipport+" during AXFR");
  if(res<0)
    throw ResolverException("Error waiting for answer from "+ipport+": "+stringerror());

  d_soacount=0;
  d_inaxfr=true;
  return 1;
}

int Resolver::getLength()
{
  int bytesLeft=2;
  unsigned char buf[2];
  
  while(bytesLeft) {
    int ret=waitForData(d_sock, 10);
    if(ret<0) {
      Utility::closesocket(d_sock);
      d_sock=-1;
      throw ResolverException("Waiting on data from remote TCP client: "+stringerror());
    }
  
    ret=recv(d_sock, reinterpret_cast< char * >( buf ) +2-bytesLeft, bytesLeft,0);
    if(ret<0)
      throw ResolverException("Trying to read data from remote TCP client: "+stringerror());
    if(!ret) 
      return -1;
    
    bytesLeft-=ret;
  }
  return buf[0]*256+buf[1];
}

int Resolver::axfrChunk(Resolver::res_t &res)
{
  if(d_soacount>1) {
    Utility::closesocket(d_sock);
    d_sock=-1;
    return 0;
  }

  // d_sock is connected and is about to spit out a packet
  int len=getLength();
  if(len<0)
    throw ResolverException("EOF trying to read axfr chunk from remote TCP client");
  
  timeoutReadn((char *)d_buf,len); 
  d_len=len;

  res=result();
  for(res_t::const_iterator i=res.begin();i!=res.end();++i)
    if(i->qtype.getCode()==QType::SOA) {
      d_soacount++;
    }

  if(d_soacount>1 && !res.empty()) // chop off the last SOA
    res.resize(res.size()-1);

  return 1;
}

Resolver::res_t Resolver::result()
{
  shared_ptr<MOADNSParser> mdp;
  
  try {
    mdp=shared_ptr<MOADNSParser>(new MOADNSParser((char*)d_buf, d_len));
  }
  catch(...) {
    throw ResolverException("resolver: unable to parse packet of "+itoa(d_len)+" bytes");
  }
  if(mdp->d_header.id != d_randomid) {
    throw ResolverException("Remote nameserver replied with wrong id");
  }
  if(mdp->d_header.rcode) {
    if(d_inaxfr)
      throw ResolverException("Remote nameserver unable/unwilling to AXFR with us: RCODE="+itoa(mdp->d_header.rcode));
    else
      throw ResolverException("Remote nameserver reported error: RCODE="+itoa(mdp->d_header.rcode));
  }
    if(!d_inaxfr) {
      if(mdp->d_header.qdcount!=1)
        throw ResolverException("resolver: received answer with wrong number of questions ("+itoa(mdp->d_header.qdcount)+")");
      
      if(mdp->d_qname != d_domain+".")
        throw ResolverException(string("resolver: received an answer to another question (")+mdp->d_qname+"!="+d_domain+".)");
    }
    
    vector<DNSResourceRecord> ret; 
    DNSResourceRecord rr;
    for(MOADNSParser::answers_t::const_iterator i=mdp->d_answers.begin(); i!=mdp->d_answers.end(); ++i) {          
      rr.qname = i->first.d_label;
      if(!rr.qname.empty())
        boost::erase_tail(rr.qname, 1); // strip .
      rr.qtype = i->first.d_type;
      rr.ttl = i->first.d_ttl;
      rr.content = i->first.d_content->getZoneRepresentation();
      rr.priority = 0;
      
      uint16_t qtype=rr.qtype.getCode();

      if(!rr.content.empty() && (qtype==QType::MX || qtype==QType::NS || qtype==QType::CNAME))
        boost::erase_tail(rr.content, 1);

      if(rr.qtype.getCode() == QType::MX) {
        vector<string> parts;
        stringtok(parts, rr.content);
        rr.priority = atoi(parts[0].c_str());
        if(parts.size() > 1)
          rr.content=parts[1];
      } else if(rr.qtype.getCode() == QType::SRV) {
        rr.priority = atoi(rr.content.c_str());
        vector<pair<string::size_type, string::size_type> > fields;
        vstringtok(fields, rr.content, " ");
        if(fields.size()==4)
          rr.content=string(rr.content.c_str() + fields[1].first, fields[3].second - fields[1].first);
      }
      ret.push_back(rr);
    }
    
    return ret;
}

void Resolver::getSoaSerial(const string &ip, const string &domain, uint32_t *serial)
{
  resolve(ip, domain.c_str(), QType::SOA);
  res_t res=result();
  if(res.empty())
    throw ResolverException("Query to '" + ip + "' for SOA of '" + domain + "' produced no answers");

  if(res[0].qtype.getCode() != QType::SOA) 
    throw ResolverException("Query to '" + ip + "' for SOA of '" + domain + "' produced a "+res[0].qtype.getName()+" record");

  vector<string>parts;
  stringtok(parts, res[0].content);
  if(parts.size()<3)
    throw ResolverException("Query to '" + ip + "' for SOA of '" + domain + "' produced an unparseable response");
  
  *serial=(uint32_t)atol(parts[2].c_str());
}

