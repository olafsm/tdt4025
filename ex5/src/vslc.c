#include <stdio.h>
#include <stdlib.h>
#include <vslc.h>


node_t *root;               // Syntax tree                  
tlhash_t *global_names;     // Symbol table        
char **string_list;         // List of strings in the source
size_t n_string_list = 8;   // Initial string list capacity (grow on demand)                                            
size_t stringc = 0;         // Initial string count

void yyparse();
void simplify_tree();
void node_print();
void create_symbol_table();
void print_symbol_table();
void destroy_subtree();

int
main ( int argc, char **argv )
{
    yyparse();

    simplify_tree ( &root, root );

    node_print ( root, 0 );

    create_symbol_table();

    print_symbol_table();
    // call function to create symbol table
	// then call function to print symbol table
    destroy_subtree ( root );
	// call function to destroy symbol table

}
