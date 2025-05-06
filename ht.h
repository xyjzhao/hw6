#ifndef HT_H
#define HT_H

#include <vector>
#include <iostream>
#include <cmath>
#include <stdexcept>
#include <functional>  // std::hash, std::equal_to
#include <utility>     // std::pair

typedef size_t HASH_INDEX_T;

// -----------------------------------------------------------------------------
// Base Prober class
// -----------------------------------------------------------------------------
template <typename KeyType>
struct Prober {
    HASH_INDEX_T start_;    // initial hash location (h1(k))
    HASH_INDEX_T m_;        // table size
    size_t      numProbes_; // probe attempts

    static const HASH_INDEX_T npos = (HASH_INDEX_T)-1;

    void init(HASH_INDEX_T start, HASH_INDEX_T m, const KeyType& key) {
        (void) key;
        start_     = start;
        m_         = m;
        numProbes_ = 0;
    }
    HASH_INDEX_T next() {
        throw std::logic_error("Not implemented...should use derived class");
    }
};

// -----------------------------------------------------------------------------
// Linear probing: h(i) = (start + i) mod m
// -----------------------------------------------------------------------------
template <typename KeyType>
struct LinearProber : public Prober<KeyType> {
    HASH_INDEX_T next() {
        if (this->numProbes_ >= this->m_)  // tried every slot?
            return this->npos;
        HASH_INDEX_T loc = (this->start_ + this->numProbes_) % this->m_;
        this->numProbes_++;
        return loc;
    }
};

// -----------------------------------------------------------------------------
// Double hashing: h(i) = (h1 + i * step) mod m, 
//   where step = modulus - (h2(k) % modulus)
// -----------------------------------------------------------------------------
template <typename KeyType, typename Hash2>
struct DoubleHashProber : public Prober<KeyType> {
    Hash2         h2_;
    HASH_INDEX_T  dhstep_;

    static const HASH_INDEX_T DOUBLE_HASH_MOD_VALUES[];
    static const int          DOUBLE_HASH_MOD_SIZE;

private:
    HASH_INDEX_T findModulusToUseFromTableSize(HASH_INDEX_T currTableSize) {
        HASH_INDEX_T modulus = DOUBLE_HASH_MOD_VALUES[0];
        for (int i = 0; i < DOUBLE_HASH_MOD_SIZE && DOUBLE_HASH_MOD_VALUES[i] < currTableSize; i++)
            modulus = DOUBLE_HASH_MOD_VALUES[i];
        return modulus;
    }

public:
    DoubleHashProber(const Hash2& h2 = Hash2())
      : h2_(h2) {}

    void init(HASH_INDEX_T start, HASH_INDEX_T m, const KeyType& key) {
        Prober<KeyType>::init(start, m, key);
        HASH_INDEX_T modulus = findModulusToUseFromTableSize(m);
        dhstep_ = modulus - (h2_(key) % modulus);
    }

    HASH_INDEX_T next() {
        if (this->numProbes_ >= this->m_)
            return this->npos;
        HASH_INDEX_T loc = (this->start_ + this->numProbes_ * dhstep_) % this->m_;
        this->numProbes_++;
        return loc;
    }
};

// static moduli list
template <typename KeyType, typename Hash2>
const HASH_INDEX_T DoubleHashProber<KeyType,Hash2>::DOUBLE_HASH_MOD_VALUES[] = {
    7, 19, 43, 89, 193, 389, 787, 1583, 3191, 6397,
    12841, 25703, 51431, 102871, 205721, 411503, 823051,
    1646221, 3292463, 6584957, 13169963, 26339921,
    52679927, 105359939, 210719881, 421439749, 842879563,
    1685759113
};
template <typename KeyType, typename Hash2>
const int DoubleHashProber<KeyType,Hash2>::DOUBLE_HASH_MOD_SIZE =
    sizeof(DoubleHashProber<KeyType,Hash2>::DOUBLE_HASH_MOD_VALUES)
    / sizeof(HASH_INDEX_T);

// -----------------------------------------------------------------------------
// Hash Table
// -----------------------------------------------------------------------------
template<
    typename K,
    typename V,
    typename Prober = LinearProber<K>,
    typename Hash   = std::hash<K>,
    typename KEqual = std::equal_to<K>
>
class HashTable {
public:
    typedef K                             KeyType;
    typedef V                             ValueType;
    typedef std::pair<KeyType,ValueType> ItemType;
    typedef Hash                          Hasher;

    struct HashItem {
        ItemType item;
        bool     deleted;
        HashItem(const ItemType& newItem)
          : item(newItem), deleted(false) {}
    };

    HashTable(
        double resizeAlpha = 0.4,
        const Prober& prober = Prober(),
        const Hasher& hash   = Hasher(),
        const KEqual& kequal = KEqual());
    ~HashTable();

    bool empty() const;
    size_t size()  const;

    void insert(const ItemType& p);
    void remove(const KeyType& key);

    ItemType const* find(const KeyType& key) const;
    ItemType*       find(const KeyType& key);
    const ValueType& at(const KeyType& key) const;
    ValueType&       at(const KeyType& key);
    const ValueType& operator[](const KeyType& key) const { return at(key); }
    ValueType&       operator[](const KeyType& key)       { return at(key); }

    void reportAll(std::ostream& out) const;
    void clearTotalProbes() { totalProbes_ = 0; }
    size_t totalProbes()  const { return totalProbes_; }

private:
    HashItem*      internalFind(const KeyType& key) const;
    HASH_INDEX_T   probe(const KeyType& key) const;
    void           resize();

    std::vector<HashItem*> table_;
    Hasher                  hash_;
    KEqual                  kequal_;
    mutable Prober          prober_;
    mutable size_t          totalProbes_;

    double    resizeAlpha_;
    size_t    elementCount_;
    size_t    deletedCount_;
    HASH_INDEX_T mIndex_;

    static const HASH_INDEX_T npos = Prober::npos;
    static const HASH_INDEX_T CAPACITIES[];
};

// prime sizes
template<typename K, typename V, typename Prober, typename Hash, typename KEqual>
const HASH_INDEX_T HashTable<K,V,Prober,Hash,KEqual>::CAPACITIES[] = {
    11, 23, 47, 97, 197, 397, 797, 1597,
    3203, 6421, 12853, 25717, 51437, 102877,
    205759, 411527, 823117, 1646237, 3292489,
    6584983, 13169977, 26339969, 52679969,
    105359969, 210719881, 421439783, 842879579,
    1685759167
};

// ----------------------------------------------------------------------------
// Implementation
// ----------------------------------------------------------------------------

template<typename K, typename V, typename Prober, typename Hash, typename KEqual>
HashTable<K,V,Prober,Hash,KEqual>::HashTable(
    double resizeAlpha,
    const Prober& prober,
    const Hasher& hash,
    const KEqual& kequal)
  : hash_(hash), kequal_(kequal), prober_(prober),
    totalProbes_(0), resizeAlpha_(resizeAlpha),
    elementCount_(0), deletedCount_(0), mIndex_(0)
{
    table_.assign(CAPACITIES[mIndex_], nullptr);
}

template<typename K, typename V, typename Prober, typename Hash, typename KEqual>
HashTable<K,V,Prober,Hash,KEqual>::~HashTable() {
    for (auto ptr : table_) delete ptr;
}

template<typename K, typename V, typename Prober, typename Hash, typename KEqual>
bool HashTable<K,V,Prober,Hash,KEqual>::empty() const {
    return elementCount_ == 0;
}

template<typename K, typename V, typename Prober, typename Hash, typename KEqual>
size_t HashTable<K,V,Prober,Hash,KEqual>::size() const {
    return elementCount_;
}

template<typename K, typename V, typename Prober, typename Hash, typename KEqual>
void HashTable<K,V,Prober,Hash,KEqual>::insert(const ItemType& p) {
    HASH_INDEX_T loc = probe(p.first);
    if (loc == npos) throw std::logic_error("HashTable full");
    if (!table_[loc]) {
        table_[loc] = new HashItem(p);
        elementCount_++;
    }
    else if (table_[loc]->deleted) {
        table_[loc]->item    = p;
        table_[loc]->deleted = false;
        elementCount_++;
        deletedCount_--;
    }
    else {
        table_[loc]->item.second = p.second;
    }
    double lf = double(elementCount_) / CAPACITIES[mIndex_];
    if (lf > resizeAlpha_) resize();
}

template<typename K, typename V, typename Prober, typename Hash, typename KEqual>
void HashTable<K,V,Prober,Hash,KEqual>::remove(const KeyType& key) {
    auto hi = internalFind(key);
    if (hi && !hi->deleted) {
        hi->deleted = true;
        elementCount_--;
        deletedCount_++;
    }
}

template<typename K, typename V, typename Prober, typename Hash, typename KEqual>
typename HashTable<K,V,Prober,Hash,KEqual>::ItemType const*
HashTable<K,V,Prober,Hash,KEqual>::find(const KeyType& key) const {
    HASH_INDEX_T h = probe(key);
    if (h==npos || !table_[h]) return nullptr;
    return &table_[h]->item;
}

template<typename K, typename V, typename Prober, typename Hash, typename KEqual>
typename HashTable<K,V,Prober,Hash,KEqual>::ItemType*
HashTable<K,V,Prober,Hash,KEqual>::find(const KeyType& key) {
    HASH_INDEX_T h = probe(key);
    if (h==npos || !table_[h]) return nullptr;
    return &table_[h]->item;
}

template<typename K, typename V, typename Prober, typename Hash, typename KEqual>
typename HashTable<K,V,Prober,Hash,KEqual>::HashItem*
HashTable<K,V,Prober,Hash,KEqual>::internalFind(const KeyType& key) const {
    HASH_INDEX_T h = probe(key);
    if (h==npos || !table_[h]) return nullptr;
    return table_[h];
}

template<typename K, typename V, typename Prober, typename Hash, typename KEqual>
const typename HashTable<K,V,Prober,Hash,KEqual>::ValueType&
HashTable<K,V,Prober,Hash,KEqual>::at(const KeyType& key) const {
    auto hi = internalFind(key);
    if (!hi) throw std::out_of_range("Bad key");
    return hi->item.second;
}

template<typename K, typename V, typename Prober, typename Hash, typename KEqual>
typename HashTable<K,V,Prober,Hash,KEqual>::ValueType&
HashTable<K,V,Prober,Hash,KEqual>::at(const KeyType& key) {
    auto hi = internalFind(key);
    if (!hi) throw std::out_of_range("Bad key");
    return hi->item.second;
}

template<typename K, typename V, typename Prober, typename Hash, typename KEqual>
HASH_INDEX_T HashTable<K,V,Prober,Hash,KEqual>::probe(const KeyType& key) const {
    HASH_INDEX_T h = hash_(key) % CAPACITIES[mIndex_];
    prober_.init(h, CAPACITIES[mIndex_], key);

    HASH_INDEX_T loc = prober_.next();
    totalProbes_++;
    while (loc != Prober::npos) {
        if (!table_[loc]) {
            return loc;
        }
        else if (!table_[loc]->deleted && kequal_(table_[loc]->item.first, key)) {
            return loc;
        }
        loc = prober_.next();
        totalProbes_++;
    }
    return Prober::npos;
}

template<typename K, typename V, typename Prober, typename Hash, typename KEqual>
void HashTable<K,V,Prober,Hash,KEqual>::resize() {
    if (mIndex_ + 1 >= sizeof(CAPACITIES)/sizeof(CAPACITIES[0]))
        throw std::logic_error("No more primes to grow to");

    auto oldTable = std::move(table_);
    mIndex_++;
    table_.assign(CAPACITIES[mIndex_], nullptr);
    elementCount_ = 0;
    deletedCount_ = 0;

    for (auto ptr : oldTable) {
        if (!ptr) continue;
        if (!ptr->deleted) {
            HASH_INDEX_T h = hash_(ptr->item.first) % CAPACITIES[mIndex_];
            prober_.init(h, CAPACITIES[mIndex_], ptr->item.first);
            HASH_INDEX_T loc = prober_.next();
            while (loc != Prober::npos && table_[loc] != nullptr)
                loc = prober_.next();
            table_[loc] = ptr;
            elementCount_++;
        } else {
            delete ptr;
        }
    }
}

template<typename K, typename V, typename Prober, typename Hash, typename KEqual>
void HashTable<K,V,Prober,Hash,KEqual>::reportAll(std::ostream& out) const {
    for (HASH_INDEX_T i = 0; i < CAPACITIES[mIndex_]; ++i) {
        if (table_[i])
            out << "Bucket " << i << ": "
                << table_[i]->item.first << " => "
                << table_[i]->item.second << "\n";
    }
}

#endif // HT_H
