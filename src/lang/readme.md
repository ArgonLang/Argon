# Grammar rules of Argon programming language

|                      |                                                              |
| -------------------- | :----------------------------------------------------------- |
| \<program\>          | \<declaration\>* %EOF%                                       |
| \<declaration\>      | \<impl_decl\> \| \<access_modifier\>                         |
| \<access_modifier\>  | ['pub'] (\<const_decl\> \| \<trait_decl\> \| \<small_decl\>) \| \<statement\> |
| \<small_decl\>       | \<var_modifier\> \| \<alias_decl\> \| \<func_decl\> \| \<struct_decl\> |
| \<alias_decl\>       | 'using' %IDENTIFIER% 'as' \<scope\>                          |
| \<const_decl\>       | 'let' %IDENTIFIER% '=' \<testlist\>                          |
| \<var_modifier\>     | \['atomic'\] \['weak'\] \<var_decl\>                         |
| \<var_decl\>         | 'var' %IDENTIFIER% \[\<var_annotation\>\]\['=' \<testlist\>\] |
| \<var_annotation\>   | ':' \<scope\>                                                |
| \<func_decl\>        | 'func' %IDENTIFIER% ['(' \<param\> ')'] \<block\>            |
| \<variadic\>         | '...' %IDENTIFIER%                                           |
| \<struct_decl\>      | 'struct' %IDENTIFIER% ['impl' \<trait_list\>] \<struct_block\> |
| \<struct_block\>     | '{' (['pub'] (\<const_decl\> \| \<var_modifier\> \| \<func_decl\>))* '}' |
| \<trait_decl\>       | 'trait' %IDENTIFIER% [':' \<trait_list\>] \<trait_block\>    |
| \<trait_block\>      | '{' (['pub'] (\<func_decl\> \| \<const_decl\>))* '}'         |
| \<trait_list\>       | \<scope\> (',' \<scope\>)*                                   |
| \<impl_decl\>        | 'impl' \<scope\> ['for' \<scope\>] \<trait_block\>           |
|                      |                                                              |
| \<statement\>        | [ %IDENTIFIER% ':' ] \<expression\><br />\| \<spawn_stmt\><br />\| \<defer_stmt\><br />\| \<return_stmt\><br />\| \<import_stmt\><br />\| \<from_import_stmt\><br />\| \<for_stmt\><br />\| \<loop_stmt\><br />\| \<if_stmt\><br />\| \<switch_stmt\><br />\| \<jmp_stmt\> |
| \<import_stmt\>      | 'import' \<scope_as_name\> (',' \<scope_as_name\>)*          |
| \<from_import_stmt\> | 'from' \<scope\> 'import' \<import_as_name\> (',' \<import_as_name\>)* |
| \<import_as_name\>   | %IDENTIFIER% ['as' %IDENTIFIER%]                             |
| \<scope_as_name\>    | \<scope\> ['as' %IDENTIFIER%]                                |
| \<spawn_stmt\>       | 'spawn' \<atom_expr\>                                        |
| \<defer_stmt\>       | 'defer' \<atom_expr\>                                        |
| \<return_stmt\>      | 'return' \<testlist\>                                        |
| \<jmp_stmt\>         | ('break' \| 'continue' \| 'goto') [%IDENTIFIER%] \| 'fallthrough' |
| \<for_stmt\>         | 'for' (<br />[\<var_decl\> \| \<expression\>] ';' \<test\> ';' \<test\><br />\| %IDENTIFIER% (',' %IDENTIFIER%)* 'in' \<expression\><br />) \<block\> |
| \<loop_stmt\>        | 'loop' [\<test\>] \<block\>                                  |
| \<if_stmt\>          | 'if' \<test\> \<block\> [('elif' \<test\> \<block\>)* 'else' \<block\>] |
| \<switch_stmt\>      | 'switch' [\<test\>] '{' \<switch_case\>* '}'                 |
| \<switch_case\>      | \<switch_label\> \<block_body\>*                             |
| \<switch_label\>     | 'case' \<test\> (';' \<test\>)* ':' \| 'default' ':'         |
| \<block\>            | '{' \<block_body\>* '}'                                      |
| \<block_body\>       | \<small_decl\> \| \<statement\>                              |
|                      |                                                              |
| \<expression\>       | \<assignment\>                                               |
| \<assignment\>       | \<testlist\> [('=' \| '+=' \| '-=' \| '*=' \| '/=') \<testlist\>] |
| \<testlist\>         | \<test\> (',' \<test\>)*                                     |
| \<test\>             | \<or_test\> (['?:' \<testlist\>] \| ['?' \<testlist\> ':' \<testlist\>]) |
| \<or_test\>          | \<and_test\> ('\|\|' \<and_test\>)*                          |
| \<and_test\>         | \<or_expr\> ('&&' \<or_expr\>)*                              |
| \<or_expr\>          | \<xor_expr\> ('\|' \<xor_expr\>)*                            |
| \<xor_expr\>         | \<and_expr\> ('^' \<and_expr\>)*                             |
| \<and_expr\>         | \<equality_expr\> ('&' \<equality_expr\>)*                   |
| \<equality_expr\>    | \<relational_expr\> (('==' \| '!=') \<relational_expr\>)*    |
| \<relational_expr\>  | \<shift_expr\> (('<' \| '>' \| '<=' \| '>=') \<shift_expr\>)* |
| \<shift_expr\>       | \<arith_expr\> (('<<' \| '>>') \<arith_expr\>)*              |
| \<arith_expr\>       | \<mult_expr\> (('+' \| '-') \<mult_expr\>)*                  |
| \<mult_expr\>        | \<unary_expr\> (('\*' \| '/' \| '%' \| '//') \<unary_expr\>)* |
| \<unary_expr\>       | ('!' \| '~' \| '-' \| '+' \| '++' \| '--') \<unary\> \| \<atom_expr\> |
| \<atom_expr\>        | \<atom\> \<trailer\>* [\<initializer\> (\<member_access\> \<trailer\>* [\<initializer\>])*]  [ '++' \| '--' ] |
| \<initializer\>      | '!{' [<br />\<test\> (',' \<test\>)* \|<br />%IDENTIFIER% ':' \<test\> (',' %IDENTIFIER% ':' \<test\>)<br />] '}' |
| \<trailers\>         | '(' \<arguments\> ')' \| \<subscript\> \| \<member_access\>  |
| \<arguments\>        | [ \<test\> (',' \<test\>)* [',' \<test\> '...'] ] \| \<test\> '...' |
| \<subscript\>        | '[' \<test\> [':' \<test\> [':' \<test\> ] ] ']'             |
| \<member_access\>    | '.' \<scope\> \| '?.' \<scope\><br />                        |
| \<atom\>             | 'false' \| 'true' \| 'nil' \| 'self'<br />\| \<number\><br />\| \<string\><br />\| \<list\><br />\| \<maporset\><br />\| \<scope\><br />\| \<arrow\> |
| \<arrow\>            | \<peap\> '=>' \<block\>                                      |
| \<peap\>             | '(' [ \<test\> (',' \<test\>)* [',' \<variadic\>] ] \| \<variadic\> ')' |
| \<list\>             | '[' [ \<test\> (',' \<test\>)* ] ']'                         |
| \<maporset\>         | '{' [<br />\<test\> ':' \<test\> ( ',' \<test\> ':' \<test\>)* \|<br />\<test\> (',' \<test\>)*<br />] '}' |
| \<scope\>            | %IDENTIFIER% ('::' %IDENTIFIER%)                             |
| \<number\>           | %NUMBER% \| %NUMBER_BIN% \| %NUMBER_OCT% \| %NUMBER_HEX% \| %DECIMAL% |
| \<string\>           | %STRING% \| %BYTE_STRING% \| %RAW_STRING%                    |
