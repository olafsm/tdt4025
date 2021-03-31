#include <vslc.h>

// Externally visible, for the generator
extern tlhash_t *global_names;
extern char **string_list;
extern size_t n_string_list,stringc;

/* External interface */

void create_symbol_table(void);
void print_symbol_table(void);
void print_symbols(void);
void print_bindings(node_t *root);
void destroy_symbol_table(void);
void find_globals(void);
void bind_names(symbol_t *function, node_t *root);
void destroy_symtab(void);

void
create_symbol_table ( void )
{
  find_globals();
  size_t n_globals = tlhash_size ( global_names );
  symbol_t *global_list[n_globals];
  tlhash_values ( global_names, (void **)&global_list );
  for ( size_t i=0; i<n_globals; i++ )
      if ( global_list[i]->type == SYM_FUNCTION )
          bind_names ( global_list[i], global_list[i]->node );
}


void
print_symbol_table ( void )
{
	print_symbols();
	print_bindings(root);
}



void
print_symbols ( void )
{
    printf ( "String table:\n" );
    for ( size_t s=0; s<stringc; s++ )
        printf  ( "%zu: %s\n", s, string_list[s] );
    printf ( "-- \n" );

    printf ( "Globals:\n" );
    size_t n_globals = tlhash_size(global_names);
    symbol_t *global_list[n_globals];
    tlhash_values ( global_names, (void **)&global_list );
    for ( size_t g=0; g<n_globals; g++ )
    {
        switch ( global_list[g]->type )
        {
            case SYM_FUNCTION:
                printf (
                    "%s: function %zu:\n",
                    global_list[g]->name, global_list[g]->seq
                );
                if ( global_list[g]->locals != NULL )
                {
                    size_t localsize = tlhash_size( global_list[g]->locals );
                    printf (
                        "\t%zu local variables, %zu are parameters:\n",
                        localsize, global_list[g]->nparms
                    );
                    symbol_t *locals[localsize];
                    tlhash_values(global_list[g]->locals, (void **)locals );
                    for ( size_t i=0; i<localsize; i++ )
                    {
                        printf ( "\t%s: ", locals[i]->name );
                        switch ( locals[i]->type )
                        {
                            case SYM_PARAMETER:
                                printf ( "parameter %zu\n", locals[i]->seq );
                                break;
                            case SYM_LOCAL_VAR:
                                printf ( "local var %zu\n", locals[i]->seq );
                                break;
                            default:
                                break;
                        }
                    }
                }
                break;
            case SYM_GLOBAL_VAR:
                printf ( "%s: global variable\n", global_list[g]->name );
                break;
            default:
                break;
        }
    }
    printf ( "-- \n" );
}


void
print_bindings ( node_t *root )
{
    if ( root == NULL )
        return;
    else if ( root->entry != NULL )
    {
        switch ( root->entry->type )
        {
            case SYM_GLOBAL_VAR: 
                printf ( "Linked global var '%s'\n", root->entry->name );
                break;
            case SYM_FUNCTION:
                printf ( "Linked function %zu ('%s')\n",
                    root->entry->seq, root->entry->name
                );
                break; 
            case SYM_PARAMETER:
                printf ( "Linked parameter %zu ('%s')\n",
                    root->entry->seq, root->entry->name
                );
                break;
            case SYM_LOCAL_VAR:
                printf ( "Linked local var %zu ('%s')\n",
                    root->entry->seq, root->entry->name
                );
                break;
        }
    } else if ( root->type == STRING_DATA ) {
        size_t string_index = *((size_t *)root->data);
        if ( string_index < stringc )
            printf ( "Linked string %zu\n", *((size_t *)root->data) );
        else
            printf ( "(Not an indexed string)\n" );
    }
    for ( size_t c=0; c<root->n_children; c++ )
        print_bindings ( root->children[c] );
}


void
destroy_symbol_table ( void )
{
      destroy_symtab();
}


void
find_globals ( void )
{
    global_names = malloc(sizeof(tlhash_t));
    tlhash_init (global_names, 8);
    size_t n_functions = 0;
    string_list = malloc(n_string_list * sizeof(char * ));
    node_t *global_list = root->children[0];

    for (int i=0; i<global_list->n_children; i++) 
    {
        node_t *global = global_list->children[i], *namelist;
        symbol_t *symbol;
        switch (global->type) 
        {
        case FUNCTION:
            symbol = malloc(sizeof(symbol_t));
            *symbol = (symbol_t) {
                .type = SYM_FUNCTION,
                .name = global->children[0]->data,
                .node = global->children[2],
                .seq = n_functions,
                .nparms = 0,
                .locals = malloc( sizeof(tlhash_t))
            };
            n_functions++;
            tlhash_init (symbol->locals, 32);
            if(global->children[1] != NULL) 
            {
                symbol->nparms = global->children[1]->n_children;
                for(int i=0;i<symbol->nparms;i++)
                {
                    node_t *param = global->children[1]->children[i];
                    symbol_t *psym = malloc(sizeof(symbol_t));
                    *psym = (symbol_t) {
                        .type = SYM_PARAMETER,
                        .name = param->data,
                        .node = NULL,
                        .seq = i,
                        .nparms = 0,
                        .locals = NULL
                    };
                    tlhash_insert(
                        symbol->locals,psym->name, strlen(psym->name),psym
                    );
                }
            }
            tlhash_insert(global_names, symbol->name, strlen(symbol->name),symbol);
            break;
        case DECLARATION:
            namelist = global->children[0];
            for(int i=0; i<namelist->n_children;i++) 
            {
                symbol = malloc(sizeof(symbol_t));
                *symbol = (symbol_t) {
                    .type = SYM_GLOBAL_VAR,
                    .name = namelist->children[i]->data,
                    .seq = 0,
                    .nparms = 0,
                    .locals = NULL
                };
                tlhash_insert(global_names, symbol->name, strlen(symbol->name), symbol);
            }
            break;
        default:
            break;
        }
    }
} 

size_t n_scopes = 1, scope_depth = 0;
tlhash_t **scopes = NULL;

static symbol_t *
lookup_local ( char *name )
{
    symbol_t *result = NULL;
    size_t depth = scope_depth;
    while ( result == NULL && depth > 0 )
    {
        depth -= 1;
        tlhash_lookup ( scopes[depth], name, strlen(name), (void **)&result );
    }
    return result;
}

void
bind_names ( symbol_t *function, node_t *root )
{
    if(root == NULL) {return;}
    node_t *namelist;
    symbol_t *entry;
    switch (root->type)
    {
        case BLOCK:
            // Initialize new scope for local variables
            if(scopes == NULL) 
            {
                scopes = malloc (n_scopes*sizeof(tlhash_t *));
            }
            tlhash_t *new_scope = malloc(sizeof(tlhash_t));
            tlhash_init (new_scope,32);
            scopes[scope_depth] = new_scope;

            scope_depth += 1;
            if(scope_depth >= n_scopes)
            {
                n_scopes *= 2;
                scopes = realloc(scopes, n_scopes*sizeof(tlhash_t **));
            }

            // bind names of children
            for(size_t c=0; c<root->n_children;c++) 
            {
                bind_names(function, root->children[c]);
            }

            // Reduce scope
            scope_depth-=1;
            tlhash_finalize(scopes[scope_depth]);
            free(scopes[scope_depth]);
            scopes[scope_depth] = NULL;
            break;
    
        case DECLARATION:
            namelist = root->children[0];
            for(int i=0;i<namelist->n_children;i++) 
            {
                node_t *varname = namelist->children[i];
                size_t local_num = tlhash_size(function->locals - function->nparms);
                symbol_t *symbol = malloc(sizeof(symbol_t));
                *symbol = (symbol_t) {
                    .type = SYM_LOCAL_VAR,
                    .name = varname->data,
                    .node = NULL,
                    .seq = local_num,
                    .nparms = 0,
                    .locals = NULL
                };
                tlhash_insert (
                    function->locals, &local_num, sizeof(size_t), symbol
                );
                tlhash_insert (
                    scopes[scope_depth-1],symbol->name,strlen(symbol->name),symbol
                );
            }
            break;
        case IDENTIFIER_DATA:
            entry = lookup_local(root->data);
            if(entry == NULL) {
                tlhash_lookup (
                    function->locals, 
                    root->data, 
                    strlen(root->data),
                    (void**)&entry
                );
            }
            if(entry == NULL) {
                tlhash_lookup (
                    global_names,
                    root->data,
                    strlen(root->data),
                    (void**)&entry
                );
            }
            if(entry == NULL) {
                fprintf(stderr, "YOU DUN GOOFED");
                exit(-1);
            }
            root->entry = entry;
            break;
        case STRING_DATA:
            string_list[stringc] = root->data;
            root->data = malloc ( sizeof(size_t) );
            *((size_t *)root->data) = stringc;
            stringc++;
            if ( stringc >= n_string_list )
            {
                n_string_list *= 2;
                string_list = realloc ( string_list, n_string_list * sizeof(char *) );
            }
            break;
        default:
            for(int i=0;i<root->n_children;i++)
            {
                bind_names(function,root->children[i]);
            }
            break;
    }
}

void
destroy_symtab ( void )
{
    for ( size_t i=0; i<stringc; i++ )
        free ( string_list[i] );
    free ( string_list );

    size_t n_globals = tlhash_size ( global_names );
    symbol_t *global_list[n_globals];
    tlhash_values ( global_names, (void **)&global_list );
    for ( size_t g=0; g<n_globals; g++ )
    {
        symbol_t *glob = global_list[g];
        if ( glob->locals != NULL )
        {
            size_t n_locals = tlhash_size ( glob->locals );
            symbol_t *locals[n_locals];
            tlhash_values ( glob->locals, (void **)&locals );
            for ( size_t l=0; l<n_locals; l++ )
                free ( locals[l] );
            tlhash_finalize ( glob->locals );
            free ( glob->locals );
        }
        free ( glob );
    }
    tlhash_finalize ( global_names );
    free ( global_names );
    free ( scopes );
}

