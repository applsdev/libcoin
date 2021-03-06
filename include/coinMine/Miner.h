/* -*-c++-*- libcoin - Copyright (C) 2012 Michael Gronager
 *
 * libcoin is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * any later version.
 *
 * libcoin is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with libcoin.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef _MINER_H_
#define _MINER_H_

#include <boost/noncopyable.hpp>
#include <boost/asio.hpp>

#include <coinMine/Export.h>

#include <coin/Address.h>
#include <coin/Key.h>

#include <coinWallet/Wallet.h>

/// The Miner class is mining for new blocks by babysitting the registered block hashers.
/// It updates the block candidate according to: https://en.bitcoin.it/wiki/Block_hashing_algorithm
/// by chooses the fastest hashing algorithm registered.
/// The Miner updates the block with transactions and timestamp every few seconds configurable by the
/// update_interval property.
/// Pr. default one miner is supplied, namely the Satoshi client cpu miner. Other miners can be written
/// following that template.

class Node;
class Block;

class COINMINE_EXPORT Miner : boost::noncopyable {
public:
    /// Construct the Miner using the a reserved wallet key. This will ensure a new key for each generated block.
    explicit Miner(Node& node, CReserveKey& reservekey);
    
    /// Construct the Miner using a public key;
    explicit Miner(Node& node, PubKey& pubkey);
    
    /// Construct the Miner using an address;
    explicit Miner(Node& node, PubKeyHash& address);
    
    /// Run the miner's io_service loop.
    void run();
    
    /// Shutdown the Miner.
    void shutdown() { _io_service.dispatch(boost::bind(&Miner::handle_stop, this)); }
    
    /// Start and stop generation
    void setGenerate(bool gen);
    
    /// Check generation
    bool getGenerate() const { return _generate; }
    
    /// Check generation performance.
    const int64 hashesPerSecond() const { return _hashes_per_second; }
    
    /// Setter and Getter for the update interval in millisec.
    const unsigned int getUpdateInterval() const { return _update_interval; }
    void setUpdateInterval(unsigned int t) { _update_interval = t; }
    
    /// Interface to a Hashing algorithm. The hashing algorithm takes a <block>, tries <nonces> hashes
    /// of the block header and returns if the proof of work condition is met (true) or all
    /// hashes has been tried (false).
    class Hasher {
    public:
        virtual bool operator()(Block& block, unsigned int nonces) = 0;

        /// Auto generated name - override if you need anything else than the classname
        virtual const std::string name() const;
        
        /// Description - optional
        virtual const std::string description() const { return ""; }
        
        /// Return true if the hashing algorithm is supported on this platform, false otherwise - mandatory.
        virtual const bool supported() const = 0;
    };
    typedef boost::shared_ptr<Hasher> hasher_ptr;
    typedef std::set<hasher_ptr> Hashers;
    
    /// Register a hashing alogorithm
    void registerHasher(hasher_ptr hasher) { _hashers.insert(hasher); }

    /// Get a const handle to the hashers, e.g. to iterate them.
    const Hashers hashers() const { return _hashers; }
    
    /// Override the automatic hasher chooser mechanish.
    void setHasher(const std::string name) { _override_name = name; }
    
private:
    /// handle_generate generates the block candidate and calls the hasher to perform a suitable step
    void handle_generate();
    
    /// handle_stop cancels all pending requests and exits the loop.
    void handle_stop() { _io_service.stop(); }
    
    /// handle_work is called when the idle timer expires
    void handle_work() {};
    
    /// fillinTransactions is based in CreateNewBlock from the Satoshi client
    void fillinTransactions(Block& block, const CBlockIndex* prev);
    
private:
    Node& _node;    
    boost::asio::io_service _io_service;
    boost::asio::deadline_timer _idle_timer;
    Hashers _hashers;
    std::string _override_name;
    unsigned int _update_interval; // milliseconds
    bool _generate;
    const PubKeyHash _address;
    const PubKey _pub_key;
    CReserveKey _reserve_key;
    uint64 _hashes_per_second;
};

#endif // _MINER_H_
