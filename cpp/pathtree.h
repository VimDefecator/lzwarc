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
        for (auto &[name, _] : children) fn(name);
    }

    template<typename Func>
    void iterFpos(Func fn) const {
        if (fpos != -1) {
            fn(fpos);
        } else {
            for (auto &[_, subtree] : children) {
                subtree.iterFpos(fn);
            }
        }
    }

    void print(
        std::ostream &out,
        bool recursive = true,
        std::string pref = ""
    ) const {
        for (auto &[name, subtree] : children) {
            out << pref
                << name
                << " : "
                << subtree.orsz
                << (recursive || subtree.children.empty() ? "\n" : " /\n");
            if (recursive) {
                subtree.print(out, true, pref + "| ");
            }
        }
    }
private:
    std::map<std::string, pathtree> children;
    uint32_t fpos = -1, orsz, arsz;
};

