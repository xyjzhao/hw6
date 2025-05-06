#ifndef HT_H
#define HT_H

#include <vector>
#include <functional>
#include <stdexcept>
#include <utility>
#include <cstddef>
#include <iostream>

// basic index type
typedef std::size_t HASH_INDEX_T;

// -----------------------------------------------------------------------------
// Prober base and derived classes
// -----------------------------------------------------------------------------

template<typename KeyType>
struct Prober {
    static const HASH_INDEX_T npos = static_cast<HASH_INDEX_T>(-1);
    HASH_INDEX_T start_;
    HASH_INDEX_T m_;
    size_t numProbes_;
    void init(HASH_INDEX_T start, HASH_INDEX_T m, const KeyType& key) {
        (void)key;
        start_ = start;
        m_ = m;
        numProbes_ = 0;
    }
    HASH_INDEX_T next() {
        throw std::logic_error("Prober::next() must be overridden");
    }
};

template<typename KeyType>
struct LinearProber : public Prober<KeyType> {
    HASH_INDEX_T next() {
        if (this->numProbes_ >= this->m_) return this->npos;
        HASH_INDEX_T loc = (this->start_ + this->numProbes_) % this->m_;
        ++this->numProbes_;
        return loc;
    }
};

template<typename KeyType, typename Hash2>
struct DoubleHashProber : public Prober<KeyType> {
    Hash2 h2_;
    HASH_INDEX_T dhstep_;
    static const HASH_INDEX_T MODS[];
    static const int MODS_COUNT;
    DoubleHashProber(const Hash2& h2 = Hash2()) : h2_(h2) {}
    void init(HASH_INDEX_T start, HASH_INDEX_T m, const KeyType& key) {
        Prober<KeyType>::init(start, m, key);
        // find largest modulus < m
        HASH_INDEX_T mod = MODS[0];
        for (int i = 0; i < MODS_COUNT && MODS[i] < m; ++i) mod = MODS[i];
        dhstep_ = mod - (h2_(key) % mod);
    }
    HASH_INDEX_T next() {
        if (this->numProbes_ >= this->m_) return this->npos;
        HASH_INDEX_T loc = (this->start_ + this->numProbes_ * dhstep_) % this->m_;
        ++this->numProbes_;
        return loc;
    }
};

template<typename KeyType, typename Hash2>
const HASH_INDEX_T DoubleHashProber<KeyType,Hash2>::MODS[] = {
    7,19,43,89,193,389,787,1583,3191,6397,
    12841,25703,51431,102871,205721,411503,823051,
    1646221,3292463,6584957,13169963,26339921,
    52679927,105359939,210719881,421439749,842879563,
    1685759113
};
template<typename KeyType, typename Hash2>
const int DoubleHashProber<KeyType,Hash2>::MODS_COUNT = sizeof(DoubleHashProber<KeyType,Hash2>::MODS)/sizeof(HASH_INDEX_T);

// -----------------------------------------------------------------------------
// HashTable with open addressing
// -----------------------------------------------------------------------------

template<
    typename K,
    typename V,
    typename Prober = LinearProber<K>,
    typename Hash = std::hash<K>,
    typename KEqual = std::equal_to<K>
>
class HashTable {
public:
    typedef K KeyType;
    typedef V ValueType;
    typedef std::pair<KeyType,ValueType> ItemType;
    struct HashItem { ItemType item; bool deleted; HashItem(const ItemType& it): item(it), deleted(false){} };

    // species default threshold = 1.0 (no auto-resize)
    HashTable(double resizeAlpha = 0.4,
              const Prober& prober = Prober(),
              const Hash& hash = Hash(),
              const KEqual& kequal = KEqual())
      : hash_(hash), kequal_(kequal), prober_(prober),
        resizeAlpha_(resizeAlpha), elementCount_(0), deletedCount_(0), mIndex_(0)
    {
        table_.assign(CAPACITIES[mIndex_], nullptr);
    }

    ~HashTable() {
        for (auto ptr : table_) delete ptr;
    }

    bool empty() const { return elementCount_ == 0; }
    size_t size() const { return elementCount_; }

    // expose table_ for testing
    std::vector<HashItem*> table_;

    // expose probe for testing
    HASH_INDEX_T probe(const KeyType& key) const {
        HASH_INDEX_T h = hash_(key) % CAPACITIES[mIndex_];
        prober_.init(h, CAPACITIES[mIndex_], key);
        HASH_INDEX_T loc = prober_.next(); ++totalProbes_;
        while (loc != Prober::npos) {
            // Stop at empty slot, deleted slot, or matching key
            if (!table_[loc] || (!table_[loc]->deleted && kequal_(table_[loc]->item.first, key)))
                return loc;
            loc = prober_.next(); ++totalProbes_;
        }
        return Prober::npos;
    }

    void insert(const ItemType& p) {
        // consider tombstones in load factor
        double lf = double(elementCount_ + deletedCount_) / CAPACITIES[mIndex_];
        if (lf >= resizeAlpha_) resize();

        HASH_INDEX_T loc = probe(p.first);
        if (loc == Prober::npos) throw std::logic_error("HashTable full");

        if (!table_[loc]) {
            table_[loc] = new HashItem(p);
            ++elementCount_;
        } else if (table_[loc]->deleted) {
            table_[loc]->item = p;
            table_[loc]->deleted = false;
            ++elementCount_;
            --deletedCount_;
        } else {
            table_[loc]->item.second = p.second;
        }
    }

    void remove(const KeyType& key) {
        HashItem* hi = internalFind(key);
        if (hi && !hi->deleted) {
            hi->deleted = true;
            --elementCount_;
            ++deletedCount_;
        }
    }

    ItemType* find(const KeyType& key) {
        auto hi = internalFind(key);
        return hi ? &hi->item : nullptr;
    }
    const ItemType* find(const KeyType& key) const {
        auto hi = internalFind(key);
        return hi ? &hi->item : nullptr;
    }

    ValueType& at(const KeyType& key) {
        auto hi = internalFind(key);
        if (!hi) throw std::out_of_range("Bad key");
        return hi->item.second;
    }
    const ValueType& at(const KeyType& key) const {
        auto hi = internalFind(key);
        if (!hi) throw std::out_of_range("Bad key");
        return hi->item.second;
    }

    // non-const operator[]: insert if missing
    ValueType& operator[](const KeyType& key) {
        HashItem* hi = internalFind(key);
        if (!hi) {
            insert({key, ValueType()});
            hi = internalFind(key);
        }
        return hi->item.second;
    }
    const ValueType& operator[](const KeyType& key) const { return at(key); }

    void reportAll(std::ostream& out) const {
        for (size_t i = 0; i < table_.size(); ++i) {
            if (table_[i] && !table_[i]->deleted)
                out << i << ": " << table_[i]->item.first
                    << " => " << table_[i]->item.second << "\n";
        }
    }

private:
    HashItem* internalFind(const KeyType& key) const {
        HASH_INDEX_T h = hash_(key) % CAPACITIES[mIndex_];
        prober_.init(h, CAPACITIES[mIndex_], key);
        HASH_INDEX_T loc = prober_.next(); ++totalProbes_;
        while (loc != Prober::npos) {
            if (!table_[loc]) return nullptr;
            if (!table_[loc]->deleted && kequal_(table_[loc]->item.first, key))
                return table_[loc];
            loc = prober_.next(); ++totalProbes_;
        }
        return nullptr;
    }

    void resize() {
        if (mIndex_ + 1 >= (sizeof(CAPACITIES)/sizeof(CAPACITIES[0])))
            throw std::logic_error("No more primes to grow to");
        auto old = std::move(table_);
        ++mIndex_;
        table_.assign(CAPACITIES[mIndex_], nullptr);
        elementCount_ = 0;
        deletedCount_ = 0;
        for (auto ptr : old) {
            if (ptr && !ptr->deleted) {
                HASH_INDEX_T loc = probe(ptr->item.first);
                table_[loc] = ptr;
                ++elementCount_;
            } else {
                delete ptr;
            }
        }
    }

    Hash hash_;
    KEqual kequal_;
    mutable Prober prober_;
    mutable size_t totalProbes_;
    double resizeAlpha_;  
    size_t elementCount_;
    size_t deletedCount_;
    size_t mIndex_;

    static const HASH_INDEX_T CAPACITIES[];
};

// static table sizes
template<typename K, typename V, typename Prober, typename Hash, typename KEqual>
const HASH_INDEX_T HashTable<K,V,Prober,Hash,KEqual>::CAPACITIES[] = {
    11,23,47,97,197,397,797,1597,
    3203,6421,12853,25717,51437,102877,
    205759,411527,823117,1646237,3292489,
    6584983,13169977,26339969,52679969,
    105359969,210719881,421439783,842879579,
    1685759113
};

#endif // HT_H
