#include <iostream>
#include <vector>
#include <map>

template<typename itemT, typename sizeT>
class pathtree {
public:
    using vecType = std::vector<itemT>;

    void addPath(const vecType &vec, sizeT size) {
        addPath(vec.cbegin(), vec.cend(), size);
    }
    void addPath(
        typename vecType::const_iterator first,
        typename vecType::const_iterator last,
        sizeT size
    ) {
        if (first != last) {
            children[*first++].addPath(first, last, size);
        }
        this->size += size;
    }

    pathtree &getChild(const itemT &item) {
        return children.at(item);
    }

    template<typename FuncT>
    void iter(FuncT fn) {
        for (auto pair : children) fn(pair.first);
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
                << child.second.size
                << (recursive || child.second.children.empty() ? "\n" : " /\n");
            if (recursive) {
                child.second.print(out, true, pref + "| ");
            }
        }
    }
private:
    std::map<itemT, pathtree<itemT, sizeT>> children;
    sizeT size;
};

