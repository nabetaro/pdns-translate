#include "dnsseckeeper.hh"
#include "dnssecinfra.hh"
#include "statbag.hh"
#include "base32.hh"
#include <boost/foreach.hpp>
#include <boost/program_options.hpp>
#include "dnsbackend.hh"
#include "ueberbackend.hh"
#include "arguments.hh"
#include "packetcache.hh"

StatBag S;
PacketCache PC;

using namespace boost;
namespace po = boost::program_options;
po::variables_map g_vm;

string s_programname="pdns_server";

ArgvMap &arg()
{
  static ArgvMap arg;
  return arg;
}


string humanTime(time_t t)
{
  char ret[256];
  struct tm tm;
  localtime_r(&t, &tm);
  strftime(ret, sizeof(ret)-1, "%c", &tm);   // %h:%M %Y-%m-%d
  return ret;
}

void loadMainConfig()
{
   static char pietje[128]="!@@SYSCONFDIR@@:";
  ::arg().set("config-dir","Location of configuration directory (pdns.conf)")=
    strcmp(pietje+1,"@@SYSCONFDIR@@:") ? pietje+strlen("@@SYSCONFDIR@@:")+1 : SYSCONFDIR;
  
  ::arg().set("launch","Which backends to launch");
  
  ::arg().set("config-name","Name of this virtual configuration - will rename the binary image")="";
  ::arg().setCmd("help","Provide a helpful message");
  //::arg().laxParse(argc,argv);

  if(::arg().mustDo("help")) {
    cerr<<"syntax:"<<endl<<endl;
    cerr<<::arg().helpstring(::arg()["help"])<<endl;
    exit(99);
  }

  if(::arg()["config-name"]!="") 
    s_programname+="-"+::arg()["config-name"];

  string configname=::arg()["config-dir"]+"/"+s_programname+".conf";
  cleanSlashes(configname);

  cerr<<"configname: '"<<configname<<"'\n";
  
  ::arg().laxFile(configname.c_str());

  BackendMakers().launch(::arg()["launch"]); // vrooooom!
  ::arg().laxFile(configname.c_str());    
  cerr<<"Backend: "<<::arg()["launch"]<<", '" << ::arg()["gmysql-dbname"] <<"'" <<endl;

  S.declare("qsize-q","Number of questions waiting for database attention");
    
  S.declare("deferred-cache-inserts","Amount of cache inserts that were deferred because of maintenance");
  S.declare("deferred-cache-lookup","Amount of cache lookups that were deferred because of maintenance");
          
  S.declare("query-cache-hit","Number of hits on the query cache");
  S.declare("query-cache-miss","Number of misses on the query cache");
  ::arg().set("max-cache-entries", "Maximum number of cache entries")="1000000";
  ::arg().set("recursor","If recursion is desired, IP address of a recursing nameserver")="no"; 
  ::arg().set("recursive-cache-ttl","Seconds to store packets in the PacketCache")="10";
  ::arg().set("cache-ttl","Seconds to store packets in the PacketCache")="20";              
  ::arg().set("negquery-cache-ttl","Seconds to store packets in the PacketCache")="60";
  ::arg().set("query-cache-ttl","Seconds to store packets in the PacketCache")="20";              
  ::arg().set("soa-refresh-default","Default SOA refresh")="10800";
  ::arg().set("soa-retry-default","Default SOA retry")="3600";
  ::arg().set("soa-expire-default","Default SOA expire")="604800";
    ::arg().setSwitch("query-logging","Hint backends that queries should be logged")="no";
  ::arg().set("soa-minimum-ttl","Default SOA mininum ttl")="3600";    
  UeberBackend::go();
}

void orderZone(DNSSECKeeper& dk, const std::string& zone)
{
  loadMainConfig();
  reportAllTypes();  
  UeberBackend* B = new UeberBackend("default");
  SOAData sd;
  
  if(!B->getSOA(zone, sd)) {
    cerr<<"No SOA!"<<endl;
    return;
  } 
  cerr<<"ID: "<<sd.domain_id<<endl;
  sd.db->list(zone, sd.domain_id);
  DNSResourceRecord rr;

  set<string> qnames;
  
  while(sd.db->get(rr)) {
  //  cerr<<rr.qname<<endl;
    qnames.insert(rr.qname);
  }
  
  string salt;
  char tmp[]={0xab, 0xcd};
  salt.assign(tmp, 2);
  
  NSEC3PARAMRecordContent ns3pr;
  dk.getNSEC3PARAM(zone, &ns3pr);
  string hashed;
  BOOST_FOREACH(const string& qname, qnames)
  {
    if(ns3pr.d_salt.empty()) // NSEC
      sd.db->updateDNSSECOrderAndAuth(sd.domain_id, zone, qname, true);
    else {
      hashed=toLower(toBase32Hex(hashQNameWithSalt(ns3pr.d_iterations, ns3pr.d_salt, qname)));
      cerr<<"'"<<qname<<"' -> '"<< hashed <<"'"<<endl;
      sd.db->updateDNSSECOrderAndAuthAbsolute(sd.domain_id, qname, hashed, true);
    }
  }
  cerr<<"Done listing"<<endl;
}

void checkZone(DNSSECKeeper& dk, const std::string& zone)
{
  loadMainConfig();
  reportAllTypes();  
  UeberBackend* B = new UeberBackend("default");
  SOAData sd;
  
  if(!B->getSOA(zone, sd)) {
    cerr<<"No SOA!"<<endl;
    return;
  } 
  cerr<<"ID: "<<sd.domain_id<<endl;
  sd.db->list(zone, sd.domain_id);
  DNSResourceRecord rr;
  uint64_t numrecords=0, numerrors=0;
  
    while(sd.db->get(rr)) {
      if(rr.qtype.getCode() == QType::MX) 
	rr.content = lexical_cast<string>(rr.priority)+" "+rr.content;
      
      try {
	shared_ptr<DNSRecordContent> drc(DNSRecordContent::mastermake(rr.qtype.getCode(), 1, rr.content));
	string tmp=drc->serialize(rr.qname);
      }
      catch(std::exception& e) 
      {
	cerr<<"Following record had a problem: "<<rr.qname<<" IN " <<rr.qtype.getName()<< " " << rr.content<<endl;
	cerr<<"Error was: "<<e.what()<<endl;
	numerrors++;
      }
      numrecords++;
    }
    cerr<<"Checked "<<numrecords<<" records, "<<numerrors<<" errors"<<endl;
  
  
}


int main(int argc, char** argv)
try
{
  po::options_description desc("Allowed options");
  desc.add_options()
    ("help,h", "produce help message")
    ("key-repository,k", po::value<string>()->default_value("./keys"), "Location of keys")
    ("verbose,v", po::value<bool>(), "be verbose")
    ("force", "force an action")
    ("commands", po::value<vector<string> >());

  po::positional_options_description p;
  p.add("commands", -1);
  po::store(po::command_line_parser(argc, argv).options(desc).positional(p).run(), g_vm);
  po::notify(g_vm);

  vector<string> cmds;

  if(g_vm.count("commands")) 
    cmds = g_vm["commands"].as<vector<string> >();

  if(cmds.empty() || g_vm.count("help")) {
    cerr<<"Usage: \npdnssec [options] [show-zone] [secure-zone] [alter-zone] [order-zone] [add-zone-key] [deactivate-zone-key] [remove-zone-key] [activate-zone-key]\n";
    cerr<<desc<<endl;
    return 0;
  }

  DNSSECKeeper dk(g_vm["key-repository"].as<string>());

  if(cmds[0] == "order-zone") {
    if(cmds.size() != 2) {
      cerr << "Error: "<<cmds[0]<<" takes exactly 1 parameter"<<endl;
      return 0;
    }
    orderZone(dk, cmds[1]);
  }
  if(cmds[0] == "check-zone") {
    if(cmds.size() != 2) {
      cerr << "Error: "<<cmds[0]<<" takes exactly 1 parameter"<<endl;
      return 0;
    }
    checkZone(dk, cmds[1]);
  }
#if 0
  else if(cmds[0] == "update-zone-keys") {
    if(cmds.size() != 2) {
      cerr << "Error: "<<cmds[0]<<" takes exactly 1 parameter"<<endl;
      return 0;
    }

    const string& zone=cmds[1];
    DNSSECPrivateKey dpk;
    
    if(!dk.haveKSKFor(zone, &dpk)) {
      cerr << "No KSK for zone '"<<zone<<"', can't update the ZSKs"<<endl;
      return 0;
    }
    DNSSECKeeper::keyset_t zskset=dk.getZSKsFor(zone);

    int inforce=0;
    time_t now = time(&now);
    
    
    if(!zskset.empty())  {
      cout<<"There were ZSKs already for zone '"<<zone<<"': "<<endl;
      
      BOOST_FOREACH(DNSSECKeeper::keyset_t::value_type value, zskset) {
        cout<<"Tag = "<<value.first.getDNSKEY().getTag()<<"\tActive: "<<value.second.active<<endl; // ", "<<humanTime(value.second.beginValidity)<<" - "<<humanTime(value.second.endValidity)<<endl;
        if(value.second.active) 
          inforce++;
        if(!value.second.active) { // was: 'expired more than two days ago'  
          cout<<"\tThis key is no longer used and too old to keep around, deleting!\n";
          dk.deleteZSKFor(zone, value.second.fname);
        } else /* if( value.second.endValidity < now  ) */{ // 'expired more than two days ago'  
          cout<<"\tThis key is no longer in active use, but needs to linger\n";
        }
      }
    }
      
    if(inforce >= 2) {
      cerr << "Two or more ZSKs were active already, not generating a third" << endl;
      return 0;
    }
    dk.addKey(zone, true, 5);
    dk.addKey(zone, true, 5, false); // not yet active

    keyset = dk.getKeys(zone);
    if(zskset.empty()) {
      cerr<<"This should not happen, still no ZSK!"<<endl;
    }

    cout<<"There are now "<<zskset.size()<<" ZSKs"<<endl;
    BOOST_FOREACH(DNSSECKeeper::keyset_t::value_type value, zskset) {
      cout<<"Tag = "<<value.first.getDNSKEY().getTag()<<"\tActive: "<<value.second.active<<endl;
    }

  }
#endif
  else if(cmds[0] == "show-zone") {
    if(cmds.size() != 2) {
      cerr << "Error: "<<cmds[0]<<" takes exactly 1 parameter"<<endl;
      return 0;
    }
    const string& zone=cmds[1];
    
    DNSSECKeeper::keyset_t keyset=dk.getKeys(zone, boost::indeterminate);

    if(keyset.empty())  {
      cerr << "No keys for zone '"<<zone<<"'."<<endl;
    }
    else {  
      cout << "keys: for zone '"<<zone<<"':"<<endl;
      BOOST_FOREACH(DNSSECKeeper::keyset_t::value_type value, keyset) {
        cout<<"ID = "<<value.second.id<<" ("<<(value.second.keyOrZone ? "KSK" : "ZSK")<<"), tag = "<<value.first.getDNSKEY().getTag()<<"\tActive: "<<value.second.active<< endl; // humanTime(value.second.beginValidity)<<" - "<<humanTime(value.second.endValidity)<<endl;
        if(value.second.keyOrZone) {
          cout<<"KSK DNSKEY = "<<zone<<" IN DNSKEY "<< value.first.getDNSKEY().getZoneRepresentation() << endl;
          cout<<"DS = "<<zone<<" IN DS "<<makeDSFromDNSKey(zone, value.first.getDNSKEY()).getZoneRepresentation() << endl << endl;
        }
      }
    }
  }
  else if(cmds[0] == "activate-zone-key") {
    const string& zone=cmds[1];
    unsigned int id=atoi(cmds[2].c_str());
    dk.activateKey(zone, id);
  }
  else if(cmds[0] == "deactivate-zone-key") {
    const string& zone=cmds[1];
    unsigned int id=atoi(cmds[2].c_str());
    dk.deactivateKey(zone, id);
  }
  else if(cmds[0] == "add-zone-key") {
    const string& zone=cmds[1];
    // need to get algorithm & ksk or zsk
    dk.addKey(zone, 1, 5, 0); 
    cerr<<"Not implemented"<<endl;
  }
  else if(cmds[0] == "remove-zone-key") {
    const string& zone=cmds[1];
    unsigned int id=atoi(cmds[2].c_str());
    dk.removeKey(zone, id);
  }
  
  else if(cmds[0] == "secure-zone") {
    if(cmds.size() != 2) {
      cerr << "Error: "<<cmds[0]<<" takes exactly 1 parameter"<<endl;
      return 0;
    }
    const string& zone=cmds[1];
    DNSSECPrivateKey dpk;
    
    if(dk.haveActiveKSKFor(zone, &dpk) && !g_vm.count("force")) {
      cerr << "There is a key already for zone '"<<zone<<"', use --force to overwrite"<<endl;
      return 0;
    }
      
    dk.secureZone(zone, 5);

    if(!dk.haveActiveKSKFor(zone, &dpk)) {
      cerr << "This should not happen, still no key!" << endl;
    }
    cout<<"Created KSK with tag "<<dpk.getDNSKEY().getTag()<<endl;
  
    DNSSECKeeper::keyset_t zskset=dk.getKeys(zone, false);

    if(!zskset.empty() && !g_vm.count("force"))  {
      cerr<<"There were ZSKs already for zone '"<<zone<<"'"<<endl;
      return 0;
    }
      
    dk.addKey(zone, false, 5);
    dk.addKey(zone, false, 5, false); // not active

    zskset = dk.getKeys(zone, false);
    if(zskset.empty()) {
      cerr<<"This should not happen, still no ZSK!"<<endl;
    }

    cout<<"There are now "<<zskset.size()<<" ZSKs"<<endl;
    BOOST_FOREACH(DNSSECKeeper::keyset_t::value_type value, zskset) {
      cout<<"id = "<<value.second.id<<", tag = "<<value.first.getDNSKEY().getTag()<<"\tActive: "<<value.second.active<<endl;
    }
  }
  else {
    cerr<<"Unknown command '"<<cmds[0]<<"'\n";
    return 1;
  }
  return 0;
}
catch(AhuException& ae) {
  cerr<<"Error: "<<ae.reason<<endl;
}
