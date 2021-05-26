#include <iostream>
#include <vector>
#include <map>

class pathtree {
public:
    using vecType = std::vector<std::string>;

    void addPath(
        const vecType &vec,
        uint32_t fpos,
        uint32_t orsz,
        uint32_t arsz
    ) {
        addPath(vec.cbegin(), vec.cend(), fpos, orsz, arsz);
    }

    void addPath(
        typename vecType::const_iterator first,
        typename vecType::const_iterator last,
        uint32_t fpos,
        uint32_t orsz,
        uint32_t arsz
    ) {
        if (first != last) {
            children[*first++].addPath(first, last, fpos, orsz, arsz);
        } else {
            this->fpos = fpos;
        }
        this->orsz += orsz;
        this->arsz += arsz;
    }

    pathtree &getChild(const std::string &key) {
        return children.at(key);
    }

    template<typename Func>
    void iterKeys(Func fn) const {
        for (auto &pair : children) fn(pair.first);
    }

    template<typename Func>
    void iterFpos(Func fn) const {
        if (fpos != -1) {
            fn(fpos);
        } else {
            for (auto &pair : children) {
                pair.second.iterFpos(fn);
            }
        }
    }

    void print(
        std::ostream &out,
        bool recursive = true,
        std::string pref = ""
    ) const {
        for (auto child : children) {
            out << pref
                << child.first
                << " : "
                << child.second.orsz
                << (recursive || child.second.children.empty() ? "\n" : " /\n");
            if (recursive) {
                child.second.print(out, true, pref + "| ");
            }
        }
    }
private:
    std::map<std::string, pathtree> children;
    uint32_t fpos = -1, orsz, arsz;
};

