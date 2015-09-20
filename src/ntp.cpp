
#ifdef WIN32
#include <winsock2.h>
#else
#include <sys/socket.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#endif
#ifndef WIN32
#include <unistd.h>
#endif

#include "netbase.h"
#include "util.h"
#include "net.h"

extern int GetRandInt(int nMax);

/*
 * NTP uses two fixed point formats.  The first (l_fp) is the "long"
 * format and is 64 bits long with the decimal between bits 31 and 32.
 * This is used for time stamps in the NTP packet header (in network
 * byte order) and for internal computations of offsets (in local host
 * byte order). We use the same structure for both signed and unsigned
 * values, which is a big hack but saves rewriting all the operators
 * twice. Just to confuse this, we also sometimes just carry the
 * fractional part in calculations, in both signed and unsigned forms.
 * Anyway, an l_fp looks like:
 *
 *    0                   1                   2                   3
 *    0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
 *   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *   |                         Integral Part                         |
 *   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *   |                         Fractional Part                       |
 *   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * REF http://www.eecis.udel.edu/~mills/database/rfc/rfc2030.txt
 */


typedef struct {
  union {
    uint32_t Xl_ui;
    int32_t Xl_i;
  } Ul_i;
  union {
    uint32_t Xl_uf;
    int32_t Xl_f;
  } Ul_f;
} l_fp;


inline void Ntp2Unix(uint32_t &n, time_t &u) {
    // Ntp's time scale starts in 1900, Unix in 1970.

    u = n - 0x83aa7e80; // 2208988800 1970 - 1900 in seconds
}

inline void ntohl_fp(l_fp *n, l_fp *h) {
    (h)->Ul_i.Xl_ui = ntohl((n)->Ul_i.Xl_ui);
    (h)->Ul_f.Xl_uf = ntohl((n)->Ul_f.Xl_uf);
}

struct pkt {
  uint8_t  li_vn_mode;     /* leap indicator, version and mode */
  uint8_t  stratum;        /* peer stratum */
  uint8_t  ppoll;          /* peer poll interval */
  int8_t  precision;      /* peer clock precision */
  uint32_t    rootdelay;      /* distance to primary clock */
  uint32_t    rootdispersion; /* clock dispersion */
  uint32_t refid;          /* reference clock ID */
  l_fp    ref;        /* time peer clock was last updated */
  l_fp    org;            /* originate time stamp */
  l_fp    rec;            /* receive time stamp */
  l_fp    xmt;            /* transmit time stamp */

  uint32_t exten[1];       /* misused */
  uint8_t  mac[5 * sizeof(uint32_t)]; /* mac */
};

int nServersCount = 112;

std::string NtpServers[112] = {
    // Microsoft
    "time.windows.com",

    // Google
    "time1.google.com",
    "time2.google.com",
    "time3.google.com",
    "time4.google.com",

    // Russian Federation
    "ntp.karelia.pro",
    "ntp.alpet.me",
    "aviel.alpet.me",
    "ntp.sampo.ru",
    "ntp.szt.ru",
    "ntp.ix.ru",
    "ntp1.stratum2.ru",
    "ntp2.stratum2.ru",
    "ntp3.stratum2.ru",
    "ntp4.stratum2.ru",
    "ntp5.stratum2.ru",
    "ntp6.stratum2.ru",
    "ntp7.stratum2.ru",
    "ntp1.stratum1.ru",
    "ntp2.stratum1.ru",
    "ntp3.stratum1.ru",
    "ntp4.stratum1.ru",
    "ntp1.vniiftri.ru",
    "ntp2.vniiftri.ru",
    "ntp3.vniiftri.ru",
    "ntp4.vniiftri.ru",
    "ntp21.vniiftri.ru",
    "ntp1.niiftri.irkutsk.ru",
    "ntp2.niiftri.irkutsk.ru",
    "vniiftri.khv.ru",
    "vniiftri2.khv.ru",
    "ntp0.zenon.net",
    "ntp.mobatime.ru",
    "0.ru.pool.ntp.org",
    "1.ru.pool.ntp.org",
    "2.ru.pool.ntp.org",
    "3.ru.pool.ntp.org",

    // United States
    "ntp-01.caltech.edu",
    "ntp-02.caltech.edu",
    "ntp-03.caltech.edu",
    "ntp-04.caltech.edu",
    "nist1-pa.ustiming.org",
    "time-a.nist.gov ",
    "time-b.nist.gov ",
    "time-c.nist.gov ",
    "time-d.nist.gov ",
    "nist1-macon.macon.ga.us",
    "nist.netservicesgroup.com",
    "nisttime.carsoncity.k12.mi.us",
    "nist1-lnk.binary.net",
    "wwv.nist.gov",
    "time-a.timefreq.bldrdoc.gov",
    "time-b.timefreq.bldrdoc.gov",
    "time-c.timefreq.bldrdoc.gov",
    "time.nist.gov",
    "utcnist.colorado.edu",
    "utcnist2.colorado.edu",
    "ntp-nist.ldsbc.net",
    "nist1-lv.ustiming.org",
    "time-nw.nist.gov",
    "nist-time-server.eoni.com",
    "nist-time-server.eoni.com",
    "ntp1.bu.edu",
    "ntp2.bu.edu",
    "ntp3.bu.edu",
    "0.us.pool.ntp.org",
    "1.us.pool.ntp.org",
    "2.us.pool.ntp.org",
    "3.us.pool.ntp.org",

    // South Africa
    "ntp1.meraka.csir.co.za",
    "ntp.is.co.za",
    "ntp2.is.co.za",
    "igubu.saix.net",
    "ntp1.neology.co.za",
    "ntp2.neology.co.za",
    "tick.meraka.csir.co.za",
    "tock.meraka.csir.co.za",
    "ntp.time.org.za",
    "ntp1.meraka.csir.co.za",
    "ntp2.meraka.csir.co.za",
    "0.za.pool.ntp.org",
    "1.za.pool.ntp.org",
    "2.za.pool.ntp.org",
    "3.za.pool.ntp.org",

    // Italy
    "ntp1.inrim.it",
    "ntp2.inrim.it",
    "0.it.pool.ntp.org",
    "1.it.pool.ntp.org",
    "2.it.pool.ntp.org",
    "3.it.pool.ntp.org",

    // Netherlands
    "ntp0.nl.net",
    "ntp1.nl.net",
    "ntp2.nl.net",
    "ntp.utwente.nl",
    "0.nl.pool.ntp.org",
    "1.nl.pool.ntp.org",
    "2.nl.pool.ntp.org",
    "3.nl.pool.ntp.org",

    // United Kingdom
    "ntp2d.mcc.ac.uk",
    "ntp2c.mcc.ac.uk",
    "ntp.exnet.com",
    "ntp.cis.strath.ac.uk",
    "ntppub.le.ac.uk",
    "0.uk.pool.ntp.org",
    "1.uk.pool.ntp.org",
    "2.uk.pool.ntp.org",
    "3.uk.pool.ntp.org",

    // Japan
    "ntp.nict.jp",
    "0.jp.pool.ntp.org",
    "1.jp.pool.ntp.org",
    "2.jp.pool.ntp.org",
    "3.jp.pool.ntp.org",

    // ... To be continued
};

bool InitWithRandom(SOCKET &sockfd, socklen_t &servlen, struct sockaddr *pcliaddr) {
    int nAttempt = 0;

    while(nAttempt < 100) {
        sockfd = -1;
        nAttempt++;

        int nServerNum = GetRandInt(nServersCount);

        std::vector<CNetAddr> vIP;
        bool fRet = LookupHost(NtpServers[nServerNum].c_str(), vIP, 10, true);
        if (!fRet)
            continue;

        struct sockaddr_in servaddr;
        servaddr.sin_family = AF_INET;
        servaddr.sin_port = htons(123);

        bool found = false;
        for(unsigned int i = 0; i < vIP.size(); i++) {
            if ((found = vIP[i].GetInAddr(&servaddr.sin_addr))) {
                break;
            }
        }

        if (!found)
            continue;

        sockfd = socket(AF_INET, SOCK_DGRAM, 0);

        if (sockfd == INVALID_SOCKET)
            continue; // socket initialization error

        if (connect(sockfd, (struct sockaddr *) &servaddr, sizeof(servaddr)) == -1 ) {
            continue; // "connection" error
        }

        *pcliaddr = *((struct sockaddr *) &servaddr);
        servlen = sizeof(servaddr);
        return true;
    }

    return false;
}

bool InitWithHost(std::string &strHostName, SOCKET &sockfd, socklen_t &servlen, struct sockaddr *pcliaddr) {
    sockfd = -1;

    std::vector<CNetAddr> vIP;
    bool fRet = LookupHost(strHostName.c_str(), vIP, 10, true);
    if (!fRet) {
        return false;
    }

    struct sockaddr_in servaddr;
    servaddr.sin_family = AF_INET;
    servaddr.sin_port = htons(123);

    bool found = false;
    for(unsigned int i = 0; i < vIP.size(); i++) {
        if ((found = vIP[i].GetInAddr(&servaddr.sin_addr))) {
            break;
        }
    }

    if (!found) {
        return false;
    }

    sockfd = socket(AF_INET, SOCK_DGRAM, 0);

    if (sockfd == INVALID_SOCKET)
        return false; // socket initialization error

    if (connect(sockfd, (struct sockaddr *) &servaddr, sizeof(servaddr)) == -1 ) {
        return false; // "connection" error
    }


    *pcliaddr = *((struct sockaddr *) &servaddr);
    servlen = sizeof(servaddr);

    return true;
}


int64_t DoReq(SOCKET sockfd, socklen_t servlen, struct sockaddr cliaddr) {
#ifdef WIN32
    u_long nOne = 1;
    if (ioctlsocket(sockfd, FIONBIO, &nOne) == SOCKET_ERROR) {
        printf("ConnectSocket() : ioctlsocket non-blocking setting failed, error %d\n", WSAGetLastError());
#else
    if (fcntl(sockfd, F_SETFL, O_NONBLOCK) == SOCKET_ERROR) {
        printf("ConnectSocket() : fcntl non-blocking setting failed, error %d\n", errno);
#endif
        return -2;
    }

    struct pkt *msg = new pkt;
    struct pkt *prt  = new pkt;

    msg->li_vn_mode=227;
    msg->stratum=0;
    msg->ppoll=4;
    msg->precision=0;
    msg->rootdelay=0;
    msg->rootdispersion=0;

    msg->ref.Ul_i.Xl_i=0;
    msg->ref.Ul_f.Xl_f=0;
    msg->org.Ul_i.Xl_i=0;
    msg->org.Ul_f.Xl_f=0;
    msg->rec.Ul_i.Xl_i=0;
    msg->rec.Ul_f.Xl_f=0;
    msg->xmt.Ul_i.Xl_i=0;
    msg->xmt.Ul_f.Xl_f=0;

    int len=48;
    int retcode = sendto(sockfd, (char *) msg, len, 0, &cliaddr, servlen);
    if (retcode < 0) {
        printf("sendto() failed: %d", retcode);
        return -3;
    }

    fd_set fdset;
    struct timeval timeout = {10, 0};
    FD_ZERO(&fdset);
    FD_SET(sockfd, &fdset);

    retcode = select(sockfd + 1, &fdset, NULL, NULL, &timeout);
    if (retcode <= 0) {
        printf("recvfrom() error");
        return -4;
    }

    recvfrom(sockfd, (char *) msg, len, 0, NULL, NULL);

    ntohl_fp(&msg->rec, &prt->rec);
    ntohl_fp(&msg->xmt, &prt->xmt);

    time_t seconds_receive;
    time_t seconds_transmit;

    Ntp2Unix(prt->rec.Ul_i.Xl_ui, seconds_receive);
    Ntp2Unix(prt->xmt.Ul_i.Xl_ui, seconds_transmit);

    delete msg;
    delete prt;

    return (seconds_receive + seconds_transmit) / 2;
}

int64_t NtpGetTime() {
    struct sockaddr cliaddr;

    SOCKET sockfd;
    socklen_t servlen;

    if (!InitWithRandom(sockfd, servlen, &cliaddr))
        return -1;

    int64_t nTime = DoReq(sockfd, servlen, cliaddr);

    closesocket(sockfd);

    return nTime;
}

int64_t NtpGetTime(CNetAddr& ip) {
    struct sockaddr cliaddr;

    SOCKET sockfd;
    socklen_t servlen;

    if (!InitWithRandom(sockfd, servlen, &cliaddr))
        return -1;

    ip = CNetAddr(((sockaddr_in *)&cliaddr)->sin_addr);
    int64_t nTime = DoReq(sockfd, servlen, cliaddr);

    closesocket(sockfd);

    return nTime;
}

int64_t NtpGetTime(std::string &strHostName)
{
    struct sockaddr cliaddr;

    SOCKET sockfd;
    socklen_t servlen;

    if (!InitWithHost(strHostName, sockfd, servlen, &cliaddr))
        return -1;

    int64_t nTime = DoReq(sockfd, servlen, cliaddr);

    closesocket(sockfd);

    return nTime;
}

void ThreadNtpSamples(void* parg)
{
    printf("ThreadNtpSamples started\n");
    vnThreadsRunning[THREAD_NTP]++;

    // Make this thread recognisable as time synchronization thread
    RenameThread("novacoin-ntp-samples");

    while (!fShutdown) {
        CNetAddr ip;
        int64_t nTime = NtpGetTime(ip);

        if (nTime > 0 && nTime != 2085978496) { // Skip the deliberately wrong timestamps
            AddTimeData(ip, nTime);
        }
        else {
            Sleep(600000); // In case of failure wait 600 seconds and then try again
            continue;
        }

        Sleep(43200000); // Sleep for 12 hours
    }

    vnThreadsRunning[THREAD_NTP]--;
    printf("ThreadNtpSamples exited\n");
}