#include "addr_name.h"
#include <cstring>
using namespace std;


void addr_name::add_item(const char *name, const char *addr_str) {
    int i;
    for (i = 0; i < _n; i++)
        if (strncmp(_pair[i].name, name, AN_NAME_SIZE) == 0)
            break;
    if (i >= AN_SIZE) throw overflow();
    strncpy(_pair[i].addr_str, addr_str, AN_ADDR_STR_SIZE);
    if (i == _n) {
        strncpy(_pair[i].name, name, AN_NAME_SIZE);
        _n++;
    }
}

char *addr_name::get_addr(const char *name) {
    for (auto &p : *this)
        if (strncmp(p.name, name, AN_NAME_SIZE) == 0)
            return p.addr_str;
    return nullptr;
}

char *addr_name::get_name(const char *addr_str) {
    for (auto &p : *this)
        if (strncmp(p.addr_str, addr_str, AN_NAME_SIZE) == 0)
            return p.name;
    return nullptr;
}
