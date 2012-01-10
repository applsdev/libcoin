// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2011 The Bitcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file license.txt or http://www.opensource.org/licenses/mit-license.php.

#include "btcNode/Endpoint.h"
#include "btc/util.h"

#ifndef _WIN32
# include <arpa/inet.h>
#endif

// Prototypes from net.h, but that header (currently) stinks, can't #include it without breaking things
bool Lookup(const char *pszName, std::vector<Endpoint>& vaddr, int nServices, int nMaxSolutions, bool fAllowLookup = false, int portDefault = 0, bool fAllowPort = false);
bool Lookup(const char *pszName, Endpoint& ep, int nServices, bool fAllowLookup = false, int portDefault = 0, bool fAllowPort = false);

static const unsigned char pchIPv4[12] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0xff, 0xff };

using namespace std;
using namespace boost::asio::ip;

// portDefault is in host order
bool Lookup(const char *pszName, vector<Endpoint>& vaddr, int nServices, int nMaxSolutions, bool fAllowLookup, int portDefault, bool fAllowPort)
{
    vaddr.clear();
    if (pszName[0] == 0)
        return false;
    int port = portDefault;
    char psz[256];
    char *pszHost = psz;
    strlcpy(psz, pszName, sizeof(psz));
    if (fAllowPort)
        {
        char* pszColon = strrchr(psz+1,':');
        char *pszPortEnd = NULL;
        int portParsed = pszColon ? strtoul(pszColon+1, &pszPortEnd, 10) : 0;
        if (pszColon && pszPortEnd && pszPortEnd[0] == 0)
            {
            if (psz[0] == '[' && pszColon[-1] == ']')
                {
                // Future: enable IPv6 colon-notation inside []
                pszHost = psz+1;
                pszColon[-1] = 0;
                }
            else
                pszColon[0] = 0;
            port = portParsed;
            if (port < 0 || port > USHRT_MAX)
                port = USHRT_MAX;
            }
        }
    
    unsigned int addrIP = inet_addr(pszHost);
    if (addrIP != INADDR_NONE)
        {
        // valid IP address passed
        vaddr.push_back(Endpoint(addrIP, port, nServices));
        return true;
        }
    
    if (!fAllowLookup)
        return false;
    
    struct hostent* phostent = gethostbyname(pszHost);
    if (!phostent)
        return false;
    
    if (phostent->h_addrtype != AF_INET)
        return false;
    
    char** ppAddr = phostent->h_addr_list;
    while (*ppAddr != NULL && vaddr.size() != nMaxSolutions)
        {
        Endpoint addr(((struct in_addr*)ppAddr[0])->s_addr, port, nServices);
        if (addr.isValid())
            vaddr.push_back(addr);
        ppAddr++;
        }
    
    return (vaddr.size() > 0);
}

// portDefault is in host order
bool Lookup(const char *pszName, Endpoint& addr, int nServices, bool fAllowLookup, int portDefault, bool fAllowPort)
{
    vector<Endpoint> vaddr;
    bool fRet = Lookup(pszName, vaddr, nServices, 1, fAllowLookup, portDefault, fAllowPort);
    if (fRet)
        addr = vaddr[0];
    return fRet;
}

Endpoint::Endpoint()
{
    init();
}

Endpoint::Endpoint(tcp::endpoint ep) : tcp::endpoint(ep) {
    _services = NODE_NETWORK;
    memcpy(_ipv6, pchIPv4, sizeof(_ipv6));
    _time = 100000000;
    _lastTry = 0;    
}

Endpoint::Endpoint(unsigned int ip, unsigned short p, uint64 services)
{
    init();
    //    address(address_v4(ip));
    //    port(htons(p == 0 ? getDefaultPort() : p));
    address(address_v4(ntohl(ip)));
    //    port(p == 0 ? getDefaultPort() : p);
    port(p);
    _services = services;
}

Endpoint::Endpoint(const struct sockaddr_in& sockaddr, uint64 services)
{
    init();
    address(address_v4(sockaddr.sin_addr.s_addr));
    port(sockaddr.sin_port);
    _services = services;
}

Endpoint::Endpoint(const char* pszIn, int portIn, bool fNameLookup, uint64 nServicesIn)
{
    init();
    Lookup(pszIn, *this, nServicesIn, fNameLookup, portIn);
}

Endpoint::Endpoint(const char* pszIn, bool fNameLookup, uint64 nServicesIn)
{
    init();
    Lookup(pszIn, *this, nServicesIn, fNameLookup, 0, true);
}

Endpoint::Endpoint(std::string strIn, int portIn, bool fNameLookup, uint64 nServicesIn)
{
    init();
    Lookup(strIn.c_str(), *this, nServicesIn, fNameLookup, portIn);
}

Endpoint::Endpoint(std::string strIn, bool fNameLookup, uint64 nServicesIn)
{
    init();
    Lookup(strIn.c_str(), *this, nServicesIn, fNameLookup, 0, true);
}

void Endpoint::init()
{
    _services = NODE_NETWORK;
    memcpy(_ipv6, pchIPv4, sizeof(_ipv6));
    //    address(address_v4(INADDR_NONE));
    //    port(htons(getDefaultPort()));
    address(address_v4(INADDR_NONE));
    //    port(getDefaultPort());
    port(0);
    _time = 100000000;
    _lastTry = 0;
}

bool operator==(const Endpoint& a, const Endpoint& b)
{
    return (memcmp(a._ipv6, b._ipv6, sizeof(a._ipv6)) == 0 &&
            a.address()   == b.address() &&
            a.port() == b.port());
}

bool operator!=(const Endpoint& a, const Endpoint& b)
{
    return (!(a == b));
}

bool operator<(const Endpoint& a, const Endpoint& b)
{
    int ret = memcmp(a._ipv6, b._ipv6, sizeof(a._ipv6));
    if (ret < 0)
        return true;
    else if (ret == 0)
    {
        if (ntohl(a.address().to_v4().to_ulong()) < ntohl(b.address().to_v4().to_ulong()))
            return true;
        else if (a.address() == b.address())
            return ntohs(a.port()) < ntohs(b.port());
    }
    return false;
}

std::vector<unsigned char> Endpoint::getKey() const
{
    CDataStream ss;
    ss.reserve(18);
    ss << FLATDATA(_ipv6) << address().to_v4().to_ulong() << port();

    #if defined(_MSC_VER) && _MSC_VER < 1300
    return std::vector<unsigned char>((unsigned char*)&ss.begin()[0], (unsigned char*)&ss.end()[0]);
    #else
    return std::vector<unsigned char>(ss.begin(), ss.end());
    #endif
}

struct sockaddr_in Endpoint::getSockAddr() const
{
    struct sockaddr_in sockaddr;
    memset(&sockaddr, 0, sizeof(sockaddr));
    sockaddr.sin_family = AF_INET;
    sockaddr.sin_addr.s_addr = address().to_v4().to_ulong();
    sockaddr.sin_port = port();
    return sockaddr;
}

bool Endpoint::isIPv4() const
{
    return (memcmp(_ipv6, pchIPv4, sizeof(pchIPv4)) == 0);
}

bool Endpoint::isRFC1918() const
{
  return isIPv4() && (getByte(3) == 10 ||
    (getByte(3) == 192 && getByte(2) == 168) ||
    (getByte(3) == 172 &&
      (getByte(2) >= 16 && getByte(2) <= 31)));
}

bool Endpoint::isRFC3927() const
{
  return isIPv4() && (getByte(3) == 169 && getByte(2) == 254);
}

bool Endpoint::isLocal() const
{
  return isIPv4() && (getByte(3) == 127 ||
      getByte(3) == 0);
}

bool Endpoint::isRoutable() const
{
    return isValid() &&
        !(isRFC1918() || isRFC3927() || isLocal());
}

bool Endpoint::isValid() const
{
    // Clean up 3-byte shifted addresses caused by garbage in size field
    // of addr messages from versions before 0.2.9 checksum.
    // Two consecutive addr messages look like this:
    // header20 vectorlen3 addr26 addr26 addr26 header20 vectorlen3 addr26 addr26 addr26...
    // so if the first length field is garbled, it reads the second batch
    // of addr misaligned by 3 bytes.
    if (memcmp(_ipv6, pchIPv4+3, sizeof(pchIPv4)-3) == 0)
        return false;

    return (address().to_v4().to_ulong() != 0 && address().to_v4().to_ulong() != INADDR_NONE && port() != htons(USHRT_MAX));
}

unsigned char Endpoint::getByte(int n) const
{
    unsigned long ip = address().to_v4().to_ulong();
    return ((unsigned char*)&ip)[n];
}

std::string Endpoint::toStringIPPort() const
{
    return strprintf("%u.%u.%u.%u:%u", getByte(3), getByte(2), getByte(1), getByte(0), port());
}

std::string Endpoint::toStringIP() const
{
    return strprintf("%u.%u.%u.%u", getByte(3), getByte(2), getByte(1), getByte(0));
}

std::string Endpoint::toStringPort() const
{
    return strprintf("%u", port());
}

std::string Endpoint::toString() const
{
    return strprintf("%u.%u.%u.%u:%u", getByte(3), getByte(2), getByte(1), getByte(0), port());
}

void Endpoint::print() const
{
    printf("Endpoint(%s)\n", toString().c_str());
}
