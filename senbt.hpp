#ifndef LIBRARY_ENBT_SENBT
#define LIBRARY_ENBT_SENBT
#include "enbt.hpp"

namespace senbt {
    // compound { "name": (value)}
    // darray [...]
    // array a[...]
    // sarray s'def'[...]  //'def' == ub, us, ui, ul, b, s, i, l, f, d
    // optional ?()
    // bit true/false   t/f
    // integer (num) / (num)(def) //def == i, I, l, L, s, S, b, B
    // float (num)f / (num)d / (num.num)F / (num.num)D
    // var_integer (num)v / (num)V  //v == 32, V == 64
    // comp_integer (num)c / (num)C  //c == 32, C == 64
    // uuid    uuid"uuid"   /   uuid'uuid' / u"uuid"  /  u'uuid'
    // string "string"  'string'
    // none //just empty string
    // log_item ((item))
    //
    //delimiter: ,
    //one line comments and multiline comments allowed( // and /**/ )
    enbt::value parse(std::string_view string);

    //consumes senbt part from string and returns enbt value
    enbt::value parse_mod(std::string_view& string);

    //set compressed to true if string is will be sent via network(skips formatting)
    std::string serialize(const enbt::value& value, bool compressed = false, bool type_erasure = false);
}


#endif /* LIBRARY_ENBT_SENBT */
