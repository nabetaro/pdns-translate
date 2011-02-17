#include "dnsparser.hh"
#include "dnswriter.hh"
#include "sstuff.hh"
#include "misc.hh"
#include "dnswriter.hh"
#include "dnsrecords.hh"
#include "statbag.hh"
#include "md5.hh"
#include "base64.hh"
#include "dnssecinfra.hh"

StatBag S;


int main(int argc, char** argv)
try
{
  reportAllTypes();

  if(argc < 4) {
    cerr<<"tsig-tests: ask a TSIG signed question, verify the TSIG signed answer"<<endl;
    cerr<<"Syntax: tsig IP-address port question question-type\n";
    exit(EXIT_FAILURE);
  }

  vector<uint8_t> packet;
  
  DNSPacketWriter pw(packet, argv[3], DNSRecordContent::TypeToNumber(argv[4]));

  pw.getHeader()->id=htons(0x4831);
  
  string key;
  B64Decode("kp4/24gyYsEzbuTVJRUMoqGFmN3LYgVDzJ/3oRSP7ys=", key);

  TSIGRecordContent trc;
  trc.d_algoName="hmac-md5.sig-alg.reg.int.";
  trc.d_time=time(0);
  trc.d_fudge=300;
  trc.d_origID=ntohs(pw.getHeader()->id);
  trc.d_eRcode=0;

  addTSIG(pw, &trc, "test", key, "", false);

  Socket sock(InterNetwork, Datagram);
  ComboAddress dest(argv[1] + (*argv[1]=='@'), atoi(argv[2]));
  
  sock.sendTo(string((char*)&*packet.begin(), (char*)&*packet.end()), dest);
  
  string reply;
  sock.recvFrom(reply, dest);

  MOADNSParser mdp(reply);
  cout<<"Reply to question for qname='"<<mdp.d_qname<<"', qtype="<<DNSRecordContent::NumberToType(mdp.d_qtype)<<endl;
  cout<<"Rcode: "<<mdp.d_header.rcode<<", RD: "<<mdp.d_header.rd<<", QR: "<<mdp.d_header.qr;
  cout<<", TC: "<<mdp.d_header.tc<<", AA: "<<mdp.d_header.aa<<", opcode: "<<mdp.d_header.opcode<<endl;

  shared_ptr<TSIGRecordContent> trc2;
  
  for(MOADNSParser::answers_t::const_iterator i=mdp.d_answers.begin(); i!=mdp.d_answers.end(); ++i) {          
    cout<<i->first.d_place-1<<"\t"<<i->first.d_label<<"\tIN\t"<<DNSRecordContent::NumberToType(i->first.d_type, i->first.d_class);
    cout<<"\t"<<i->first.d_ttl<<"\t"<< i->first.d_content->getZoneRepresentation()<<"\n";
    
    if(i->first.d_type == QType::TSIG)
      trc2 = boost::dynamic_pointer_cast<TSIGRecordContent>(i->first.d_content);
  }

  if(mdp.getTSIGPos()) {    
    string message = makeTSIGMessageFromTSIGPacket(reply, mdp.getTSIGPos(), "test", trc, trc.d_mac, false); // insert our question MAC
    
    string hmac2=calculateMD5HMAC(key, message);
    cerr<<"Calculated mac: "<<Base64Encode(hmac2)<<endl;
    if(hmac2 == trc2->d_mac)
      cerr<<"MATCH!"<<endl;
    else 
      cerr<<"Mismatch!"<<endl;
  }
}
catch(std::exception &e)
{
  cerr<<"Fatal: "<<e.what()<<endl;
}
