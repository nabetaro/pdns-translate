#ifndef PDNSDNSSECKEEPER_HH
#define PDNSDNSSECKEEPER_HH
#include <string>
#include <polarssl/rsa.h>
#include <string.h>
#include <vector>
#include "dnsrecords.hh"

#define PDNSSEC_MI(x) mpi_init(&d_context.x, 0)
#define PDNSSEC_MC(x) PDNSSEC_MI(x); mpi_copy(&d_context.x, const_cast<mpi*>(&orig.d_context.x))
#define PDNSSEC_MF(x) mpi_free(&d_context.x, 0)

class RSAContext
{
public:
  RSAContext()
  {
    memset(&d_context, 0, sizeof(d_context));
    PDNSSEC_MI(N); 
    PDNSSEC_MI(E); PDNSSEC_MI(D); PDNSSEC_MI(P); PDNSSEC_MI(Q); PDNSSEC_MI(DP); PDNSSEC_MI(DQ); PDNSSEC_MI(QP); PDNSSEC_MI(RN); PDNSSEC_MI(RP); PDNSSEC_MI(RQ);
  }

  ~RSAContext()
  {
    PDNSSEC_MF(N); 
    PDNSSEC_MF(E); PDNSSEC_MF(D); PDNSSEC_MF(P); PDNSSEC_MF(Q); PDNSSEC_MF(DP); PDNSSEC_MF(DQ); PDNSSEC_MF(QP); PDNSSEC_MF(RN); PDNSSEC_MF(RP); PDNSSEC_MF(RQ);
  }

  RSAContext(const RSAContext& orig) 
  {
    d_context.ver = orig.d_context.ver;
    d_context.len = orig.d_context.len;

    d_context.padding = orig.d_context.padding;
    d_context.hash_id = orig.d_context.hash_id;
    d_context.f_rng = orig.d_context.f_rng;
    d_context.p_rng = orig.d_context.p_rng;
    PDNSSEC_MC(N); 
    PDNSSEC_MC(E); PDNSSEC_MC(D); PDNSSEC_MC(P); PDNSSEC_MC(Q); PDNSSEC_MC(DP); PDNSSEC_MC(DQ); PDNSSEC_MC(QP); PDNSSEC_MC(RN); PDNSSEC_MC(RP); PDNSSEC_MC(RQ);
  }

  RSAContext& operator=(const RSAContext& orig) 
  {
    d_context.ver = orig.d_context.ver;
    d_context.len = orig.d_context.len;

    d_context.padding = orig.d_context.padding;
    d_context.hash_id = orig.d_context.hash_id;
    d_context.f_rng = orig.d_context.f_rng;
    d_context.p_rng = orig.d_context.p_rng;

    PDNSSEC_MC(N); 
    PDNSSEC_MC(E); PDNSSEC_MC(D); PDNSSEC_MC(P); PDNSSEC_MC(Q); PDNSSEC_MC(DP); PDNSSEC_MC(DQ); PDNSSEC_MC(QP); PDNSSEC_MC(RN); PDNSSEC_MC(RP); PDNSSEC_MC(RQ);
    return *this;
  }

  rsa_context& getContext()
  {
    return d_context;
  }

  void create(unsigned int bits);
  std::string convertToISC();

private:
  rsa_context d_context;
};

#undef PDNSSEC_MC
#undef PDNSSEC_MI
#undef PDNSSEC_MF

struct DNSSECPrivateKey
{
  uint16_t getTag();
  
  RSAContext d_key;
  DNSKEYRecordContent getDNSKEY();
  uint8_t d_algorithm;
};

class DNSSECKeeper
{
public:
  struct KeyMetaData
  {
	unsigned int id;
    bool active;
    string fname;
  }; 
  typedef std::vector<std::pair<DNSSECPrivateKey, KeyMetaData> > zskset_t;
   
public:
  explicit DNSSECKeeper(const std::string& dirname) : d_dirname(dirname){}

  bool haveKSKFor(const std::string& zone, DNSSECPrivateKey* ksk=0);
  
  zskset_t getZSKsFor(const std::string& zone, bool all=false);
  void addZSKFor(const std::string& zname, int algorithm, bool next=false);
  
  void deleteZSKFor(const std::string& zname, const std::string& fname);

  void secureZone(const std::string& fname, int algorithm);

  bool getNSEC3PARAM(const std::string& zname, NSEC3PARAMRecordContent* n3p=0);
  void setNSEC3PARAM(const std::string& zname, const NSEC3PARAMRecordContent* n3p);
  static unsigned int getNextKeyIDFromDir(const std::string& dirname);
private:
  std::string d_dirname;
};

#endif
