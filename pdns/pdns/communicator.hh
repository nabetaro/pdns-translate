/*
    PowerDNS Versatile Database Driven Nameserver
    Copyright (C) 2002  PowerDNS.COM BV

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/
#ifndef PDNS_COMMUNICATOR_HH
#define PDNS_COMMUNICATOR_HH

#include <pthread.h>
#include <string>
#include <semaphore.h>
#include <queue>
#include <list>

#ifndef WIN32
# include <unistd.h>
# include <fcntl.h>
# include <netdb.h>
#endif // WIN32

#include "lock.hh"
#include "packethandler.hh"

using namespace std;

struct SuckRequest
{
  string domain;
  string master;
};

class NotificationQueue
{
public:
  void add(const string &domain, const string &ip)
  {
    NotificationRequest nr;
    nr.domain   = domain;
    nr.ip       = ip;
    nr.attempts = 0;
    nr.id       = Utility::random()%0xffff;
    nr.next     = time(0);

    d_nqueue.push_back(nr);
  }
  
  bool removeIf(const string &remote, u_int16_t id, const string &domain)
  {
    for(d_nqueue_t::iterator i=d_nqueue.begin();i!=d_nqueue.end();++i) {
      //      cout<<i->id<<" "<<id<<endl;
      //cout<<i->ip<<" "<<remote<<endl;
      //cout<<i->domain<<" "<<domain<<endl;

      if(i->id==id && i->ip==remote && i->domain==domain) {
	d_nqueue.erase(i);
	return true;
      }
    }
    return false;
  }

  bool getOne(string &domain, string &ip, u_int16_t *id, bool &purged)
  {
    for(d_nqueue_t::iterator i=d_nqueue.begin();i!=d_nqueue.end();++i) 
      if(i->next <= time(0)) {
	i->attempts++;
	purged=false;
	i->next=time(0)+1+(1<<i->attempts);
	domain=i->domain;
	ip=i->ip;
	*id=i->id;
	purged=false;
	if(i->attempts>4) {
	  purged=true;
	  d_nqueue.erase(i);
	}
	return true;
      }
    return false;
  }
  
  time_t earliest()
  {
    time_t early=1<<31-1; // y2038 problem lurking here :-)
    for(d_nqueue_t::const_iterator i=d_nqueue.begin();i!=d_nqueue.end();++i) 
      early=min(early,i->next);
    return early-time(0);
  }

private:
  struct NotificationRequest
  {
    string domain;
    string ip;
    int attempts;
    u_int16_t id;
    time_t next;
  };

  typedef list<NotificationRequest>d_nqueue_t;
  d_nqueue_t d_nqueue;

};

/** this class contains a thread that communicates with other nameserver and does housekeeping.
    Initially, it is notified only of zones that need to be pulled in because they have been updated. */

class CommunicatorClass
{
public:
  CommunicatorClass() 
  {
    pthread_mutex_init(&d_lock,0);
    pthread_mutex_init(&d_holelock,0);
//    sem_init(&d_suck_sem,0,0);
//    sem_init(&d_any_sem,0,0);
    d_tickinterval=60;
    d_masterschanged=d_slaveschanged=true;
  }
  int doNotifications();    
  void go()
  {
    pthread_t tid;
    pthread_create(&tid,0,&launchhelper,this);
  }

  void drillHole(const string &domain, const string &ip);
  bool justNotified(const string &domain, const string &ip);
  void addSuckRequest(const string &domain, const string &master, bool priority=false);
  void notify(const string &domain, const string &ip);
  void mainloop();
  static void *launchhelper(void *p)
  {
    static_cast<CommunicatorClass *>(p)->mainloop();
    return 0;
  }
  bool notifyDomain(const string &domain);
private:
  void makeNotifySocket();
  void queueNotifyDomain(const string &domain, DNSBackend *B);
  int d_nsock;
  map<pair<string,string>,time_t>d_holes;
  pthread_mutex_t d_holelock;
  void suck(const string &domain, const string &remote);
  void slaveRefresh(PacketHandler *P);
  void masterUpdateCheck(PacketHandler *P);
  pthread_mutex_t d_lock;
  std::deque<SuckRequest> d_suckdomains;
  bool d_havepriosuckrequest;
  Semaphore d_suck_sem;
  Semaphore d_any_sem;
  int d_tickinterval;
  NotificationQueue d_nq;
  bool d_masterschanged, d_slaveschanged;
};

#endif
