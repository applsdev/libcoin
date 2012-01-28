#include "coin/KeyStore.h"
#include "coinWallet/CryptoKeyStore.h"

#include <boost/foreach.hpp>

bool CCryptoKeyStore::SetCrypted()
{
    CRITICAL_BLOCK(cs_KeyStore)
    {
    if (fUseCrypto)
        return true;
    if (!mapKeys.empty())
        return false;
    fUseCrypto = true;
    }
    return true;
}

PubKey CCryptoKeyStore::GenerateNewKey()
{
    RandAddSeedPerfmon();
    CKey key;
    key.MakeNewKey();
    if (!AddKey(key))
        throw std::runtime_error("CCryptoKeyStore::GenerateNewKey() : AddKey failed");
    return key.GetPubKey();
}

bool CCryptoKeyStore::Unlock(const CKeyingMaterial& vMasterKeyIn)
{
    CRITICAL_BLOCK(cs_KeyStore)
    {
    if (!SetCrypted())
        return false;
    
    CryptedKeyMap::const_iterator mi = mapCryptedKeys.begin();
    for (; mi != mapCryptedKeys.end(); ++mi)
        {
        const PubKey &vchPubKey = (*mi).second.first;
        const std::vector<unsigned char> &vchCryptedSecret = (*mi).second.second;
        CSecret vchSecret;
        if(!DecryptSecret(vMasterKeyIn, vchCryptedSecret, Hash(vchPubKey.begin(), vchPubKey.end()), vchSecret))
            return false;
        CKey key;
        key.SetSecret(vchSecret);
        if (key.GetPubKey() == vchPubKey)
            break;
        return false;
        }
    vMasterKey = vMasterKeyIn;
    }
    return true;
}

bool CCryptoKeyStore::AddKey(const CKey& key)
{
    CRITICAL_BLOCK(cs_KeyStore)
    {
    if (!IsCrypted())
        return CBasicKeyStore::AddKey(key);
    
    if (IsLocked())
        return false;
    
    std::vector<unsigned char> vchCryptedSecret;
    PubKey vchPubKey = key.GetPubKey();
    if (!EncryptSecret(vMasterKey, key.GetSecret(), Hash(vchPubKey.begin(), vchPubKey.end()), vchCryptedSecret))
        return false;
    
    if (!AddCryptedKey(key.GetPubKey(), vchCryptedSecret))
        return false;
    }
    return true;
}


bool CCryptoKeyStore::AddCryptedKey(const PubKey &vchPubKey, const std::vector<unsigned char> &vchCryptedSecret)
{
    CRITICAL_BLOCK(cs_KeyStore)
    {
    if (!SetCrypted())
        return false;
    
    mapCryptedKeys[ChainAddress(_id, vchPubKey)] = make_pair(vchPubKey, vchCryptedSecret);
    }
    return true;
}

bool CCryptoKeyStore::GetKey(const ChainAddress &address, CKey& keyOut) const
{
    CRITICAL_BLOCK(cs_KeyStore)
    {
    if (!IsCrypted())
        return CBasicKeyStore::GetKey(address, keyOut);
    
    CryptedKeyMap::const_iterator mi = mapCryptedKeys.find(address);
    if (mi != mapCryptedKeys.end())
        {
        const PubKey &vchPubKey = (*mi).second.first;
        const std::vector<unsigned char> &vchCryptedSecret = (*mi).second.second;
        CSecret vchSecret;
        if (!DecryptSecret(vMasterKey, vchCryptedSecret, Hash(vchPubKey.begin(), vchPubKey.end()), vchSecret))
            return false;
        keyOut.SetSecret(vchSecret);
        return true;
        }
    }
    return false;
}

bool CCryptoKeyStore::GetPubKey(const ChainAddress &address, PubKey& vchPubKeyOut) const
{
    CRITICAL_BLOCK(cs_KeyStore)
    {
    if (!IsCrypted())
        return CKeyStore::GetPubKey(address, vchPubKeyOut);
    
    CryptedKeyMap::const_iterator mi = mapCryptedKeys.find(address);
    if (mi != mapCryptedKeys.end())
        {
        vchPubKeyOut = (*mi).second.first;
        return true;
        }
    }
    return false;
}

bool CCryptoKeyStore::EncryptKeys(CKeyingMaterial& vMasterKeyIn)
{
    CRITICAL_BLOCK(cs_KeyStore)
    {
    if (!mapCryptedKeys.empty() || IsCrypted())
        return false;
    
    fUseCrypto = true;
    CKey key;
    BOOST_FOREACH(KeyMap::value_type& mKey, mapKeys)
        {
        if (!key.SetSecret(mKey.second))
            return false;
        const PubKey vchPubKey = key.GetPubKey();
        std::vector<unsigned char> vchCryptedSecret;
        if (!EncryptSecret(vMasterKeyIn, key.GetSecret(), Hash(vchPubKey.begin(), vchPubKey.end()), vchCryptedSecret))
            return false;
        if (!AddCryptedKey(vchPubKey, vchCryptedSecret))
            return false;
        }
    mapKeys.clear();
    }
    return true;
}