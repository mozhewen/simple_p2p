#include <exception>
using namespace std;


#define AN_ADDR_STR_SIZE 128
#define AN_NAME_SIZE 16
#define AN_SIZE 100

struct an_pair{
    char addr_str[AN_ADDR_STR_SIZE];
    char name[AN_NAME_SIZE];
};

class addr_name {
private:
    an_pair _pair[AN_SIZE];
    int _n;

public:
    addr_name():_n(0) {}

    class iterator {
    private:
        an_pair *_ptr;
    public:
        inline iterator(an_pair *ptr): _ptr(ptr) {}
        inline an_pair &operator *() {
            return *_ptr;
        }
        inline bool operator !=(const iterator &b) {
            return _ptr != b._ptr;
        }
        inline iterator operator++() {
            _ptr++;
            return iterator(_ptr);
        }
    };
    inline iterator begin() {
        return iterator(_pair);
    }
    inline iterator end() {
        return iterator(_pair + _n);
    }

    class overflow: exception {
    public:
        const char *what() const noexcept {
            return "addr_name: list size exceeds AN_SIZE";
        }
    };
    void add_item(const char *name, const char *addr_str);
    char *get_addr(const char *name);
    char *get_name(const char *addr_str);
};
