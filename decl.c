/* Process declarations and variables for C compiler.
   Copyright (C) 1987 Free Software Foundation, Inc.

This file is part of GNU CC.

GNU CC is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY.  No author or distributor
accepts responsibility to anyone for the consequences of using it
or for whether it serves any particular purpose or works at all,
unless he says so in writing.  Refer to the GNU CC General Public
License for full details.

Everyone is granted permission to copy, modify and redistribute
GNU CC, but only under the conditions described in the
GNU CC General Public License.   A copy of this license is
supposed to have been given to you along with GNU CC so you
can know your rights and responsibilities.  It should be in a
file named COPYING.  Among other things, the copyright notice
and this notice must be preserved on all copies.  */


/* Process declarations and symbol lookup for C front end.
   Also constructs types; the standard scalar types at initialization,
   and structure, union, array and enum types when they are declared.  */

/* ??? not all decl nodes are given the most useful possible
   line numbers.  For example, the CONST_DECLs for enum values.  */

#include "config.h"
#include "parse.h"
#include "tree.h"
#include "c-tree.h"

/* In grokdeclarator, distinguish syntactic contexts of declarators.  */
enum decl_context
{ NORMAL,			/* Ordinary declaration */
  PARM,				/* Declaration of parm before function body */
  FIELD,			/* Declaration inside struct or union */
  TYPENAME};			/* Typename (inside cast or sizeof)  */

#define NULL 0
#define min(X,Y) ((X) < (Y) ? (X) : (Y))

static tree grokparms (), grokdeclarator ();
static tree make_index_type ();
static void builtin_function ();

static tree lookup_labelname ();
static tree lookup_tag ();
static tree lookup_name_current_level ();

/* a node which has tree code ERROR_MARK, and whose type is itself.
   All erroneous expressions are replaced with this node.  All functions
   that accept nodes as arguments should avoid generating error messages
   if this node is one of the arguments, since it is undesirable to get
   multiple error messages from one error in the input.  */

tree error_mark_node;

/* INTEGER_TYPE and REAL_TYPE nodes for the standard data types */

tree short_integer_type_node;
tree integer_type_node;
tree long_integer_type_node;

tree short_unsigned_type_node;
tree unsigned_type_node;
tree long_unsigned_type_node;

tree unsigned_char_type_node;
tree char_type_node;

tree float_type_node;
tree double_type_node;
tree long_double_type_node;

/* a VOID_TYPE node.  */

tree void_type_node;

/* A node for type `void *'.  */

tree ptr_type_node;

/* A node for type `char *'.  */

tree string_type_node;

/* Type `char[256]' or something like it.
   Used when an array of char is needed and the size is irrelevant.  */

tree char_array_type_node;

/* type `int ()' -- used for implicit declaration of functions.  */

tree default_function_type;

/* function types `double (double)' and `double (double, double)', etc.  */

tree double_ftype_double, double_ftype_double_double;
tree int_ftype_int, long_ftype_long;

/* Function type `void (void *, void *, int)' and similar ones */

tree void_ftype_ptr_ptr_int, int_ftype_ptr_ptr_int, void_ftype_ptr_int_int;

/* Two expressions that are constants with value zero.
   The first is of type `int', the second of type `void *'.  */

tree integer_zero_node;
tree null_pointer_node;

/* A node for the integer constant 1.  */

tree integer_one_node;

/* An identifier whose name is <value>.  This is used as the "name"
   of the RESULT_DECLs for values of functions.  */

tree value_identifier;

/* Enumeration type currently being built, or 0 if not doing that now.  */

tree current_enum_type;

/* Default value for next enumerator of enumeration type currently being read,
   or undefined at other times.  */

int enum_next_value;

/* Parsing a function declarator leaves a list of parameter names here.
   If the declarator is the beginning of a function definition,
   the names are stored into the function declaration when it is created.  */

static tree last_function_parm_names;

/* A list (chain of TREE_LIST nodes) of all LABEL_STMTs in the function
   that have names.  Here so we can clear out their names' definitions
   at the end of the function.  */

static tree named_labels;

/* A list of all GOTO_STMT nodes in the current function,
   so we can fill in the LABEL_STMTs they go to once all are defined.  */

static tree all_gotos;

/* Set to 0 at beginning of a function definition, set to 1 if
   a return statement that specifies a return value is seen.  */

int current_function_returns_value;

/* For each binding contour we allocate a binding_level structure
 * which records the names defined in that contour.
 * Contours include:
 *  0) the global one
 *  1) one for each function definition,
 *     where internal declarations of the parameters appear.
 *  2) one for each compound statement,
 *     to record its declarations.
 *
 * The current meaning of a name can be found by searching the levels from
 * the current one out to the global one.
 */

/* Note that the information in the `names' component of the global contour
   is duplicated in the IDENTIFIER_GLOBAL_VALUEs of all identifiers.  */

static struct binding_level
  {
    /* A chain of _DECL nodes for all variables, constants, functions,
       and typedef types.  These are in the reverse of the order supplied.
     */
    tree names;

    /* A list of structure, union and enum definitions,
     * for looking up tag names.
     * It is a chain of TREE_LIST nodes, each of whose TREE_PURPOSE is a name,
     * or NULL_TREE; and whose TREE_VALUE is a RECORD_TYPE, UNION_TYPE,
     * or ENUMERAL_TYPE node.
     */
    tree tags;

    /* For each level, a list of shadowed outer-level local definitions
       to be restored when this level is popped.
       Each link is a TREE_LIST whose TREE_PURPOSE is an identifier and
       whose TREE_VALUE is its old definition (a kind of ..._DECL node).  */
    tree shadowed;

    /* The binding level which this one is contained in (inherits from).  */
    struct binding_level *level_chain;
  };

#define NULL_BINDING_LEVEL (struct binding_level *) NULL
  
/* The binding level currently in effect.  */

static struct binding_level *current_binding_level;

/* A chain of binding_level structures awaiting reuse.  */

static struct binding_level *free_binding_level;

/* The outermost binding level, for names of file scope.
   This is created when the compiler is started and exists
   through the entire run.  */

static struct binding_level *global_binding_level;

/* Binding level structures are initialized by copying this one.  */

static struct binding_level clear_binding_level =
{NULL, NULL, NULL, NULL};

/* Create a new `struct binding_level'.  */

static
struct binding_level *
make_binding_level ()
{
  /* NOSTRICT */
  return (struct binding_level *) xmalloc (sizeof (struct binding_level));
}

/* Enter a new binding level.  */

void
pushlevel ()
{
  register struct binding_level *newlevel = NULL_BINDING_LEVEL;

  /* If this is the top level of a function,
     just make sure that ALL_GOTOS and NAMED_LABELS are 0.
     They should have been set to 0 at the end of the previous function.  */

  if (current_binding_level == global_binding_level)
    {
      if (all_gotos || named_labels)
	abort ();
    }

  /* Reuse or create a struct for this binding level.  */

  if (free_binding_level)
    {
      newlevel = free_binding_level;
      free_binding_level = free_binding_level->level_chain;
    }
  else
    {
      newlevel = make_binding_level ();
    }

  /* Add this level to the front of the chain (stack) of levels that
     are active.  */

  *newlevel = clear_binding_level;
  newlevel->level_chain = current_binding_level;
  current_binding_level = newlevel;
}

/* Exit a binding level.  */

void
poplevel ()
{
  register tree link;

  /* Clear out the meanings of the local variables of this level.  */

  for (link = current_binding_level->names; link; link = TREE_CHAIN (link))
    IDENTIFIER_LOCAL_VALUE (DECL_NAME (link)) = 0;

  /* Restore all name-meanings of the outer levels
     that were shadowed by this level.  */

  for (link = current_binding_level->shadowed; link; link = TREE_CHAIN (link))
    IDENTIFIER_LOCAL_VALUE (TREE_PURPOSE (link)) = TREE_VALUE (link);

  /* If the level being exited is the top level of a function,
     match all goto statements with their labels.  */

  if (current_binding_level->level_chain == global_binding_level)
    {
      /* Search for labels for any unresolved gotos.  */

      for (link = all_gotos; link; link = TREE_CHAIN (link))
	{
	  register tree stmt = TREE_VALUE (link);
	  register tree label = lookup_labelname (STMT_BODY (stmt));

	  if (label)
	    STMT_BODY (stmt) = label;
	  else
	    {
	      yylineerror (STMT_SOURCE_LINE (stmt),
			   "no label %s visible for goto",
			   IDENTIFIER_POINTER (STMT_BODY (stmt)));
	      STMT_BODY (stmt) = NULL_TREE;
	    }
	}

      /* Then clear out the definitions of all label names,
	 since their scopes end here.  */

      for (link = named_labels; link; link = TREE_CHAIN (link))
	IDENTIFIER_LABEL_VALUE (DECL_NAME (STMT_BODY (TREE_VALUE (link)))) = 0;

      named_labels = 0;
      all_gotos = 0;
    }

  /* Pop the current level, and free the structure for reuse.  */

  {
    register struct binding_level *level = current_binding_level;
    current_binding_level = current_binding_level->level_chain;

    level->level_chain = free_binding_level;
    free_binding_level = level;
  }
}

/* Push a definition of struct, union or enum tag "name".
   "type" should be the type node.
   Note that the definition may really be just a forward reference.
   In that case, the TYPE_SIZE will be zero.  */

void
pushtag (name, type)
     tree name, type;
{
  register tree t;

  /* If the type has a name, check for duplicate definitions of the name. */

  if (name)
    {
      for (t = current_binding_level->tags; t; t = TREE_CHAIN (t))
	if (TREE_PURPOSE (t) == name)
	  {
	    yyerror ("redeclaration of struct, union or enum tag %s",
		     IDENTIFIER_POINTER (name));
	    return;
	  }

      /* Record the identifier as the type's name if it has none.  */

      if (TYPE_NAME (type) == 0)
	TYPE_NAME (type) = name;
    }

  current_binding_level->tags
    = tree_cons (name, type, current_binding_level->tags);
}


/* Handle when a new declaration X has the same name as an old one T
   in the same binding contour.
   May alter the old decl to say what the new one says.

   Returns the old decl T if the two are more or less compatible;
   returns the new one X if they are thoroughly alien.  */

static tree
duplicate_decls (x, t)
     register tree x, t;
{
  /* At top level in file, if the old definition is "tentative" and
     this one is close enough, no error.  */
  if (! allowed_redeclaration (x, t,
			       current_binding_level == global_binding_level))
    yylineerror (DECL_SOURCE_LINE (x), "redeclaration of %s",
		 IDENTIFIER_POINTER (DECL_NAME (x)));

  /* Install latest semantics.  */
  if (TREE_CODE (t) == TREE_CODE (x))
    {
      bcopy ((char *) x + sizeof (struct tree_shared),
	     (char *) t + sizeof (struct tree_shared),
	     sizeof (struct tree_decl) - sizeof (struct tree_shared));
      TREE_TYPE (t) = TREE_TYPE (x);
      DECL_ARGUMENTS (t) = DECL_ARGUMENTS (x);
      DECL_RESULT (t) = DECL_RESULT (x);
      DECL_SOURCE_FILE (t) = DECL_SOURCE_FILE (x);
      DECL_SOURCE_LINE (t) = DECL_SOURCE_LINE (x);
      TREE_STATIC (t) = TREE_STATIC (x);
      TREE_EXTERNAL (t) = TREE_EXTERNAL (x);
      TREE_PUBLIC (t) = TREE_PUBLIC (x);
      if (DECL_INITIAL (x))
	DECL_INITIAL (t) = DECL_INITIAL (x);
      return t;
    }
  return x;
}

/* Record a decl-node X as belonging to the current lexical scope.
   Check for errors (such as an incompatible declaration for the same
   name already seen in the same scope).

   Returns either X or an old decl for the same name.
   If an old decl is returned, it has been smashed
   to agree with what X says.  */

tree
pushdecl (x)
     tree x;
{
  register tree t;
  register tree name = DECL_NAME (x);

  if (name)
    {
      t = lookup_name_current_level (name);
      if (t)
	return duplicate_decls (x, t);

      /* If declaring a type as a typedef, and the type has no known
	 typedef name, install this TYPE_DECL as its typedef name.  */
      if (TREE_CODE (x) == TYPE_DECL)
	if (TYPE_NAME (TREE_TYPE (x)) == 0
	    || TREE_CODE (TYPE_NAME (TREE_TYPE (x))) != TYPE_DECL)
	  TYPE_NAME (TREE_TYPE (x)) = x;
    }

  /* This name is new.
     Install the new declaration and return it.  */
  if (current_binding_level == global_binding_level)
    {
      IDENTIFIER_GLOBAL_VALUE (name) = x;
    }
  else
    {
      /* If storing a local value, there may already be one (inherited).
	 If so, record it for restoration when this binding level ends.  */
      if (IDENTIFIER_LOCAL_VALUE (name))
	current_binding_level->shadowed
	  = tree_cons (name, IDENTIFIER_LOCAL_VALUE (name),
		       current_binding_level->shadowed);
      IDENTIFIER_LOCAL_VALUE (name) = x;
    }

  /* Put decls on list in reverse order.
     We will reverse them later if necessary.  */
  current_binding_level->names = chainon (x, current_binding_level->names);

  return x;
}

/* Record a C label name.  X is a LABEL_STMT and its STMT_BODY is a LABEL_DECL.
   That is where we find the name.  */

void
pushlabel (x)
     tree x;
{
  register tree decl = STMT_BODY (x);

  if (0 == DECL_NAME (decl))
    return;

  if (IDENTIFIER_LABEL_VALUE (DECL_NAME (decl)))
    yyerror ("duplicate label %s",
	     IDENTIFIER_POINTER (DECL_NAME (decl)));
  else
    IDENTIFIER_LABEL_VALUE (DECL_NAME (decl)) = decl;

  named_labels
    = tree_cons (NULL_TREE, x, named_labels);
}

/* Record the goto statement X on the list of all gotos in the
   current function.  The list is used to find them all at
   the end of the function so that their target labels can be found then.  */

void
pushgoto (x)
     tree x;
{
  all_gotos = tree_cons (NULL_TREE, x, all_gotos);
}

/* Return the list of declarations of the current level.
   They are pushed on the list in reverse order since that is easiest.
   We reverse them to the correct order here.  */

tree
getdecls ()
{
  return
    current_binding_level->names = nreverse (current_binding_level->names);
}

/* Return the list of type-tags (for structs, etc) of the current level.  */

tree
gettags ()
{
  return current_binding_level->tags;
}

/* Store the list of declarations of the current level.
   This is done for the parameter declarations of a function being defined,
   after they are modified in the light of any missing parameters.  */

void
storedecls (decls)
     tree decls;
{
  current_binding_level->names = decls;
}

/* Given NAME, an IDENTIFIER_NODE,
   return the structure (or union or enum) definition for that name.
   Searches binding levels from BINDING_LEVEL up to the global level.
   If THISLEVEL_ONLY is nonzero, searches only the specified context.
   FORM says which kind of type the caller wants;
   it is RECORD_TYPE or UNION_TYPE or ENUMERAL_TYPE.
   If the wrong kind of type is found, an error is reported.  */

static tree
lookup_tag (form, name, binding_level, thislevel_only)
     enum tree_code form;
     struct binding_level *binding_level;
     tree name;
     int thislevel_only;
{
  register struct binding_level *level;

  for (level = binding_level; level; level = level->level_chain)
    {
      register tree tail;
      for (tail = level->tags; tail; tail = TREE_CHAIN (tail))
	{
	  if (TREE_PURPOSE (tail) == name)
	    {
	      if (TREE_CODE (TREE_VALUE (tail)) != form)
		{
		  /* Definition isn't the kind we were looking for.  */
		  yyerror ("%s defined as wrong kind of tag",
			   IDENTIFIER_POINTER (name));
		}
	      return TREE_VALUE (tail);
	    }
	}
      if (thislevel_only)
	return NULL_TREE;
    }
  return NULL_TREE;
}

/* Look up NAME in the current binding level and its superiors
   in the namespace of variables, functions and typedefs.
   Return a ..._DECL node of some kind representing its definition,
   or return 0 if it is undefined.  */

tree
lookup_name (name)
     tree name;
{
  if (current_binding_level != global_binding_level
      && IDENTIFIER_LOCAL_VALUE (name))
    return IDENTIFIER_LOCAL_VALUE (name);
  return IDENTIFIER_GLOBAL_VALUE (name);
}

/* Similar to `lookup_name' but look only at current binding level.  */

static tree
lookup_name_current_level (name)
     tree name;
{
  register tree t;

  if (current_binding_level == global_binding_level)
    return IDENTIFIER_GLOBAL_VALUE (name);

  if (IDENTIFIER_LOCAL_VALUE (name) == 0)
    return 0;

  for (t = current_binding_level->names; t; t = TREE_CHAIN (t))
    if (DECL_NAME (t) == name)
      break;

  return t;
}

/* Return the definition of NAME as a label (a LABEL-DECL node),
   or 0 if it has no definition as a label.  */

static
tree
lookup_labelname (name)
     tree name;
{
  return IDENTIFIER_LABEL_VALUE (name);
}

/* Create a DECL_... node of code CODE, name NAME and data type TYPE.
   STATICP nonzero means this is declared `static' in the C sense;
   EXTERNP means it is declared `extern' in the C sense.
   The name's definition is *not* entered in the symbol table.
   
   The source file and line number are left 0.
   layout_decl is used to set up the decl's storage layout.
   are initialized to 0 or null pointers.  */

tree
build_decl (code, name, type, staticp, externp)
     enum tree_code code;
     tree name, type;
     int staticp, externp;
{
  register tree t;

  t = make_node (code);

/*  if (type == error_mark_node)
    type = integer_type_node; */
/* That is not done, deliberately, so that having error_mark_node
   as the type can suppress useless errors in the use of this variable.  */

  DECL_NAME (t) = name;
  TREE_TYPE (t) = type;
  DECL_ARGUMENTS (t) = NULL_TREE;
  DECL_INITIAL (t) = NULL_TREE;
  if (externp)
    TREE_EXTERNAL (t) = 1;
  if (staticp)
    TREE_STATIC (t) = 1;

  if (current_binding_level == global_binding_level)
    {
      if (!TREE_STATIC (t))
	TREE_PUBLIC (t) = 1;
      TREE_STATIC (t) = (code != FUNCTION_DECL);
    }

  if (TREE_EXTERNAL (t))
    TREE_STATIC (t) = 0;

  if (code == VAR_DECL || code == PARM_DECL || code == RESULT_DECL)
    layout_decl (t, 0);
  else if (code == FUNCTION_DECL)
    /* all functions considered external unless def in this file */
    {
      TREE_EXTERNAL (t) = 1;
      DECL_MODE (t) = FUNCTION_MODE;
    }
  return t;
}

/* Create a LABEL_STMT statement node containing a LABEL_DECL named NAME.
   NAME may be a null pointer.
   FILE ane LINENUM say where in the source the label is.
   CONTEXT is the LET_STMT node which is the context of this label name.
   The label name definition is entered in the symbol table.  */

tree
build_label (filename, line, name, context)
     char *filename;
     int line;
     tree name;
     tree context;
{
  register tree t = make_node (LABEL_STMT);
  register tree decl = build_decl (LABEL_DECL, name, NULL_TREE, 1, 0);

  STMT_SOURCE_FILE (t) = filename;
  STMT_SOURCE_LINE (t) = line;
  STMT_BODY (t) = decl;
  DECL_SOURCE_FILE (decl) = filename;
  DECL_SOURCE_LINE (decl) = line;
  DECL_MODE (decl) = VOIDmode;
  DECL_CONTEXT (decl) = context;
  pushlabel (t);
  return t;
}

/* Store the data into a LET_STMT node.
   Each C braced grouping with declarations is represented
   by a LET_STMT node.  The node is created when the open-brace is read,
   but the contents to put in it are not known until the close-brace.
   This function is called at the time of the close-brace
   to install the proper contents.

   BLOCK is the LET_STMT node itself.
   DCLS is the chain of declarations within the grouping.
   TAGS is the chain of struct, union and enum tags defined within it.
   STMTS is the chain of statements making up the inside of the grouping.  */

finish_block (block, dcls, tags, stmts)
     tree block, dcls, tags, stmts;
{
  register tree tem;

  /* In each decl, record the block it belongs to.  */
  for (tem = dcls; tem; tem = TREE_CHAIN (tem))
    DECL_CONTEXT (tem) = block;

  STMT_VARS (block) = dcls;
  STMT_BODY (block) = stmts;
  STMT_TYPE_TAGS (block) = tags;
}

finish_tree ()
{
}

/* Create the predefined scalar types of C,
   and some nodes representing standard constants (0, 1, (void *)0).
   Initialize the global binding level.
   Make definitions for built-in primitive functions.  */

void
init_decl_processing ()
{
  register tree endlink;

  named_labels = NULL;
  all_gotos = NULL;
  current_binding_level = NULL_BINDING_LEVEL;
  free_binding_level = NULL_BINDING_LEVEL;
  pushlevel ();	/* make the binding_level structure for global names */
  global_binding_level = current_binding_level;

  value_identifier = get_identifier ("<value>");

  /* This must be the first type made and laid out,
     so that it will get used as the type for expressions
     for the sizes of types.  */

  integer_type_node = make_signed_type (BITS_PER_WORD);
  pushdecl (build_decl (TYPE_DECL, ridpointers[(int) RID_INT],
			integer_type_node, 1, 0));

  error_mark_node = make_node (ERROR_MARK);
  TREE_TYPE (error_mark_node) = error_mark_node;

  short_integer_type_node = make_signed_type (BITS_PER_UNIT * min (UNITS_PER_WORD / 2, 2));
  pushdecl (build_decl (TYPE_DECL, get_identifier ("short int"),
			short_integer_type_node, 1, 0));

  long_integer_type_node = make_signed_type (BITS_PER_WORD);
  pushdecl (build_decl (TYPE_DECL, get_identifier ("long int"),
			long_integer_type_node, 1, 0));

  short_unsigned_type_node = make_unsigned_type (BITS_PER_UNIT * min (UNITS_PER_WORD / 2, 2));
  pushdecl (build_decl (TYPE_DECL, get_identifier ("short unsigned int"),
			short_unsigned_type_node, 1, 0));

  unsigned_type_node = make_unsigned_type (BITS_PER_WORD);
  pushdecl (build_decl (TYPE_DECL, get_identifier ("unsigned int"),
			unsigned_type_node, 1, 0));

  long_unsigned_type_node = make_unsigned_type (BITS_PER_WORD);
  pushdecl (build_decl (TYPE_DECL, get_identifier ("long unsigned int"),
			long_unsigned_type_node, 1, 0));

  char_type_node = make_signed_type (BITS_PER_UNIT);
  pushdecl (build_decl (TYPE_DECL, ridpointers[(int) RID_CHAR],
			char_type_node, 1, 0));

  unsigned_char_type_node = make_unsigned_type (BITS_PER_UNIT);
  pushdecl (build_decl (TYPE_DECL, get_identifier ("unsigned char"),
			unsigned_char_type_node, 1, 0));

  float_type_node = make_node (REAL_TYPE);
  TYPE_PRECISION (float_type_node) = BITS_PER_WORD;
  pushdecl (build_decl (TYPE_DECL, ridpointers[(int) RID_FLOAT],
			float_type_node, 1, 0));
  layout_type (float_type_node);

  double_type_node = make_node (REAL_TYPE);
  TYPE_PRECISION (double_type_node) = 2 * BITS_PER_WORD;
  pushdecl (build_decl (TYPE_DECL, ridpointers[(int) RID_DOUBLE],
			double_type_node, 1, 0));
  layout_type (double_type_node);

  long_double_type_node = make_node (REAL_TYPE);
  TYPE_PRECISION (long_double_type_node) = 2 * BITS_PER_WORD;
  pushdecl (build_decl (TYPE_DECL, get_identifier ("long double"),
			long_double_type_node, 1, 0));
  layout_type (long_double_type_node);

  integer_zero_node = build_int_2 (0, 0);
  TREE_TYPE (integer_zero_node) = integer_type_node;
  integer_one_node = build_int_2 (1, 0);
  TREE_TYPE (integer_one_node) = integer_type_node;

  void_type_node = make_node (VOID_TYPE);
  pushdecl (build_decl (TYPE_DECL,
			ridpointers[(int) RID_VOID], void_type_node,
			1, 0));
  layout_type (void_type_node);	/* Uses integer_zero_node */

  null_pointer_node = build_int_2 (0, 0);
  TREE_TYPE (null_pointer_node) = build_pointer_type (void_type_node);
  layout_type (TREE_TYPE (null_pointer_node));

  string_type_node = build_pointer_type (char_type_node);
  layout_type (string_type_node);

  /* make a type for arrays of 256 characters.
     256 is picked randomly because we have a type for integers from 0 to 255.
     With luck nothing will ever really depend on the length of this
     array type.  */
  char_array_type_node
    = build_array_type (char_type_node, unsigned_char_type_node);
  layout_type (char_array_type_node);

  default_function_type
    = build_function_type (integer_type_node, NULL_TREE);
  layout_type (default_function_type);

  ptr_type_node = build_pointer_type (void_type_node);
  endlink = tree_cons (NULL_TREE, void_type_node, NULL_TREE);

  double_ftype_double
    = build_function_type (double_type_node,
			   tree_cons (NULL_TREE, double_type_node, endlink));

  double_ftype_double_double
    = build_function_type (double_type_node,
			   tree_cons (NULL_TREE, double_type_node,
				      tree_cons (NULL_TREE,
						 double_type_node, endlink)));

  int_ftype_int
    = build_function_type (integer_type_node,
			   tree_cons (NULL_TREE, integer_type_node, endlink));

  long_ftype_long
    = build_function_type (long_integer_type_node,
			   tree_cons (NULL_TREE,
				      long_integer_type_node, endlink));

  void_ftype_ptr_ptr_int
    = build_function_type (void_type_node,
			   tree_cons (NULL_TREE, ptr_type_node,
				      tree_cons (NULL_TREE, ptr_type_node,
						 tree_cons (NULL_TREE,
							    integer_type_node,
							    endlink))));

  int_ftype_ptr_ptr_int
    = build_function_type (integer_type_node,
			   tree_cons (NULL_TREE, ptr_type_node,
				      tree_cons (NULL_TREE, ptr_type_node,
						 tree_cons (NULL_TREE,
							    integer_type_node,
							    endlink))));

  void_ftype_ptr_int_int
    = build_function_type (void_type_node,
			   tree_cons (NULL_TREE, ptr_type_node,
				      tree_cons (NULL_TREE, integer_type_node,
						 tree_cons (NULL_TREE,
							    integer_type_node,
							    endlink))));

  builtin_function ("_builtin_alloca",
		    build_function_type (ptr_type_node,
					 tree_cons (NULL_TREE,
						    integer_type_node,
						    endlink)),
		    BUILT_IN_ALLOCA);

  builtin_function ("_builtin_abs", int_ftype_int, BUILT_IN_ABS);
  builtin_function ("_builtin_fabs", double_ftype_double, BUILT_IN_FABS);
  builtin_function ("_builtin_labs", long_ftype_long, BUILT_IN_LABS);
/*  builtin_function ("_builtin_div", default_ftype, BUILT_IN_DIV);
  builtin_function ("_builtin_ldiv", default_ftype, BUILT_IN_LDIV); */
  builtin_function ("_builtin_ffloor", double_ftype_double, BUILT_IN_FFLOOR);
  builtin_function ("_builtin_fceil", double_ftype_double, BUILT_IN_FCEIL);
  builtin_function ("_builtin_fmod", double_ftype_double_double, BUILT_IN_FMOD);
  builtin_function ("_builtin_frem", double_ftype_double_double, BUILT_IN_FREM);
  builtin_function ("_builtin_memcpy", void_ftype_ptr_ptr_int, BUILT_IN_MEMCPY);
  builtin_function ("_builtin_memcmp", int_ftype_ptr_ptr_int, BUILT_IN_MEMCMP);
  builtin_function ("_builtin_memset", void_ftype_ptr_int_int, BUILT_IN_MEMSET);
  builtin_function ("_builtin_fsqrt", double_ftype_double, BUILT_IN_FSQRT);
  builtin_function ("_builtin_getexp", double_ftype_double, BUILT_IN_GETEXP);
  builtin_function ("_builtin_getman", double_ftype_double, BUILT_IN_GETMAN);
}

/* Make a definition for a builtin function named NAME and whose data type
   is TYPE.  TYPE should be a function type with argument types.
   FUNCTION_CODE tells later passes how to compile calls to this function.
   See tree.h for its possible values.  */

static void
builtin_function (name, type, function_code)
     char *name;
     tree type;
     enum built_in_function function_code;
{
  tree decl = build_decl (FUNCTION_DECL, get_identifier (name), type, 0, 1);
  make_function_rtl (decl);
  pushdecl (decl);
  DECL_SET_FUNCTION_CODE (decl, function_code);
}

/* Validate a structure, union or enum type: make sure it
   is not just a forward reference.
   If it is valid, return it.  Otherwise, return error_mark_node.
   Callers may want to use the returned value instead of the original type.  */

tree
resolve_tags (type)
     tree type;
{
  register char *errmsg = NULL;

  switch (TREE_CODE (type))
    {
    case RECORD_TYPE:
      errmsg = "undefined struct tag %s";
      break;

    case UNION_TYPE:
      errmsg = "undefined union tag %s";
      break;

    case ENUMERAL_TYPE:
      errmsg = "undefined enum tag %s";
      break;

    default:
      return type;
    }

  if (TYPE_SIZE (type) != 0)
    return type;

  yyerror (errmsg, IDENTIFIER_POINTER (TYPE_NAME (type)));
  return error_mark_node;
}

/* Called when a declaration is seen that contains no names to declare.
   If its type is a reference to a structure, union or enum inherited
   from a containing scope, shadow that tag name for the current scope
   with a forward reference.
   If its type defines a new named structure or union
   or defines an enum, it is valid but we need not do anything here.
   Otherwise, it is an error.  */

void
shadow_tag (declspecs)
     tree declspecs;
{
  register tree link;

  for (link = declspecs; link; link = TREE_CHAIN (link))
    {
      register tree value = TREE_VALUE (link);
      register enum tree_code code = TREE_CODE (value);
      if ((code == RECORD_TYPE || code == UNION_TYPE || code == ENUMERAL_TYPE)
	  && TYPE_SIZE (value) != 0)
	{
	  register tree name = TYPE_NAME (value);
	  register tree t = lookup_tag (code, name, current_binding_level, 1);
	  if (t == 0)
	    {
	      t = make_node (code);
	      pushtag (name, t);
	      return;
	    }
	  if (name != 0 || code == ENUMERAL_TYPE)
	    return;
	}
    }
  warning ("empty declaration");
}

/* Decode a "typename", such as "int **", returning a ..._TYPE node.  */

tree
groktypename (typename)
     tree typename;
{
  return grokdeclarator (TREE_VALUE (typename),
			 TREE_PURPOSE (typename),
			 TYPENAME);
}

/* Decode a declarator in an ordinary declaration or data definition.
   This is called as soon as the type information and variable name
   have been parsed, before parsing the initializer if any.
   Here we create the ..._DECL node, fill in its type,
   and put it on the list of decls for the current context.
   The ..._DECL node is returned as the value.

   Exception: for arrays where the length is not specified,
   the type is left null, to be filled in by `finish_decl'.

   Function definitions do not come here; they go to start_function
   instead.  However, external and forward declarations of functions
   do go through here.  Structure field declarations are done by
   grokfield and not through here.  */

tree
start_decl (declarator, declspecs, initialized)
     tree declspecs, declarator;
     int initialized;
{
  register tree decl = grokdeclarator (declarator, declspecs, NORMAL);
  if (initialized)
    TREE_EXTERNAL (decl) = 0;
  return pushdecl (decl);
}

/* Finish processing of a declaration;
   install its line number and initial value.
   If the length of an array type is not known before,
   it must be determined now, from the initial value, or it is an error.  */

void
finish_decl (filename, line, decl, init)
     char *filename;
     int line;
     tree decl, init;
{
  register tree type = TREE_TYPE (decl);

  if (init) store_init_value (decl, init);

  /* deduce size of array from initialization, if not already known */

  if (TREE_CODE (type) == ARRAY_TYPE
      && TYPE_DOMAIN (type) == 0)
    {
      register tree maxindex = NULL_TREE;

      if (init)
	{
	  /* Note MAXINDEX  is really the maximum index,
	     one less than the size.  */
	  if (TREE_CODE (init) == ADDR_EXPR
	      && TREE_CODE (TREE_OPERAND (init, 0)) == STRING_CST)
	    maxindex = build_int_2 (TREE_STRING_LENGTH (TREE_OPERAND (init, 0)) - 1, 0);
	  else if (TREE_CODE (init) == CONSTRUCTOR)
	    {
	      int nelts = list_length (TREE_OPERAND (DECL_INITIAL (decl), 0));
	      maxindex = build_int_2 (nelts - 1, 0);
	    }
	}

      if (!maxindex)
	{
	  if (!pedantic)
	    TREE_EXTERNAL (decl) = 1;
	  else if (!TREE_EXTERNAL (decl))
	    {
	      yylineerror (line, "size missing in array declaration");
	      maxindex = build_int_2 (0, 0);
	    }
	}

      if (maxindex)
	{
	  if (pedantic && integer_zerop (maxindex))
	    yylineerror (line, "zero-size array");
	  TYPE_DOMAIN (type) = make_index_type (maxindex);
	  if (!TREE_TYPE (maxindex))
	    TREE_TYPE (maxindex) = TYPE_DOMAIN (type);
	}

      layout_type (type);

      layout_decl (decl, 0);
    }

  /* Output the assembler code for the variable.  */

  rest_of_compilation (decl, current_binding_level == global_binding_level);
}

/* Given declspecs and a declarator,
   determine the name and type of the object declared.
   DECLSPECS is a chain of tree_list nodes whose value fields
    are the storage classes and type specifiers.

   DECL_CONTEXT says which syntactic context this declaration is in:
   NORMAL for most contexts.  Make a VAR_DECL or FUNCTION_DECL or TYPE_DECL.
   PARM for a paramater declaration (either within a function prototype
    or before a function body).  Make a PARM_DECL.
   TYPENAME if for a typename (in a cast or sizeof).
    Don't make a DECL node; just return the type.
   FIELD for a struct or union field; make a FIELD_DECL.

   In the TYPENAME case, DECLARATOR is really an absolute declarator.
   It may also be so in the PARM case, for a prototype where the
   argument type is specified but not the name.  */

static tree
grokdeclarator (declarator, declspecs, decl_context)
     tree declspecs;
     tree declarator;
     enum decl_context decl_context;
{
  int specbits = 0;
  tree spec;
  tree type = NULL_TREE;
  int longlong = 0;
  int constp;
  int volatilep;
  int explicit_int = 0;

  /* Anything declared one level down from the top level
     must be one of the parameters of a function
     (because the body is at least two levels down).  */

  if (decl_context == NORMAL
      && current_binding_level->level_chain == global_binding_level)
    decl_context = PARM;

  /* Look through the decl specs and record which ones appear.
     Some typespecs are defined as built-in typenames.
     Others, the ones that are modifiers of other types,
     are represented by bits in SPECBITS: set the bits for
     the modifiers that appear.  Storage class keywords are also in SPECBITS.

     If there is a typedef name or a type, store the type in TYPE.
     This includes builtin typedefs such as `int'.

     Set EXPLICIT_INT if the type is `int' or `char' and did not
     come from a user typedef.

     Set LONGLONG if `long' is mentioned twice.  */

  for (spec = declspecs; spec; spec = TREE_CHAIN (spec))
    {
      register int i;
      register tree id = TREE_VALUE (spec);

      if (id == ridpointers[(int) RID_INT]
	  || id == ridpointers[(int) RID_CHAR])
	explicit_int = 1;

      for (i = (int) RID_FIRST_MODIFIER; i < (int) RID_MAX; i++)
	{
	  if (ridpointers[i] == id)
	    {
	      if (i == (int) RID_LONG && specbits & (1<<i))
		longlong = 1;
	      specbits |= 1 << i;
	      goto found;
	    }
	}
      if (type) yyerror ("two or more data types in declaration");
      else if (TREE_CODE (id) == RECORD_TYPE || TREE_CODE (id) == UNION_TYPE || TREE_CODE (id) == ENUMERAL_TYPE)
	type = id;
      else
	{
	  register tree t = lookup_name (id);
	  if (!t || TREE_CODE (t) != TYPE_DECL)
	    yyerror ("%s fails to be a typedef or built in type",
		     IDENTIFIER_POINTER (id));
	  else type = TREE_TYPE (t);
	}

      found: {}
    }

  /* No type at all: default to `int', and set EXPLICIT_INT
     because it was not a user-defined typedef.  */

  if (type == 0)
    {
      explicit_int = 1;
      type = integer_type_node;
    }

  /* Now process the modifiers that were specified
     and check for invalid combinations.  */

  /* Long double is a special combination.  */

  if ((specbits & 1 << RID_LONG) && type == double_type_node)
    {
      specbits &= ~ (1 << RID_LONG);
      type = long_double_type_node;
    }

  /* Check all other uses of type modifiers.  */

  if (specbits & ((1 << RID_LONG) | (1 << RID_SHORT)
		  | (1 << RID_UNSIGNED) | (1 << RID_SIGNED)))
    {
      if (!explicit_int)
	yyerror ("long, short, signed or unsigned used invalidly");
      else if ((specbits & 1 << RID_LONG) && (specbits & 1 << RID_SHORT))
	yyerror ("long and short specified together");
      else if ((specbits & 1 << RID_SIGNED) && (specbits & 1 << RID_UNSIGNED))
	yyerror ("signed and unsigned specified together");
      else
	{
	  if (specbits & 1 << RID_UNSIGNED)
	    {
	      if (specbits & 1 << RID_LONG)
	        type = long_unsigned_type_node;
	      else if (specbits & 1 << RID_SHORT)
		type = short_unsigned_type_node;
	      else if (type == char_type_node)
		type = unsigned_char_type_node;
	      else
		type = unsigned_type_node;
	    }
	  else if (specbits & 1 << RID_LONG)
	    type = long_integer_type_node;
	  else if (specbits & 1 << RID_SHORT)
	    type = short_integer_type_node;
	}
    }
 
  /* Set CONSTP if this declaration is `const', whether by
     explicit specification or via a typedef.
     Likewise for VOLATILEP.  */

  constp = (specbits & 1 << RID_CONST) | TREE_READONLY (type);
  volatilep = (specbits & 1 << RID_VOLATILE) | TREE_VOLATILE (type);
  type = TYPE_MAIN_VARIANT (type);

  /* Warn if two storage classes are given. Default to `auto'.  */

  {
    int nclasses = 0;

    if (specbits & 1 << RID_AUTO) nclasses++;
    if (specbits & 1 << RID_STATIC) nclasses++;
    if (specbits & 1 << RID_EXTERN) nclasses++;
    if (specbits & 1 << RID_REGISTER) nclasses++;
    if (specbits & 1 << RID_TYPEDEF) nclasses++;

    if (nclasses == 0) specbits |= 1 << RID_AUTO;

    /* Warn about storage classes that are invalid for certain
       kinds of declarations (parameters, typenames, etc.).  */

    if (nclasses > 1)
      yyerror ("two or more storage classes in declaration");
    else if (decl_context != NORMAL && nclasses > 0)
      {
	if (decl_context == PARM && specbits & 1 << RID_REGISTER)
	  ;
	else if (decl_context == FIELD)
	  yyerror ("storage class specified in structure field");
	else yyerror (decl_context == PARM
		      ? "storage class specified in parameter list"
		      : "storage class specified in typename");
      }
    else if (current_binding_level == global_binding_level
	     && nclasses > 0)
      {
	if (specbits & (1 << RID_AUTO))
	  yyerror ("auto specified in external declaration");
	if (specbits & (1 << RID_REGISTER))
	  yyerror ("auto specified in external declaration");
      }
  }
	   
  /* Now figure out the structure of the declarator proper.
     Descend through it, creating more complex types, until we reach
     the declared identifier (or NULL_TREE, in an absolute declarator).  */

  while (declarator && TREE_CODE (declarator) != IDENTIFIER_NODE)
    {
      /* Each level of DECLARATOR is either an ARRAY_REF (for ...[..]),
	 an INDIRECT_REF (for *...),
	 a CALL_EXPR (for ...(...)),
	 an identifier (for the name being declared)
	 or a null pointer (for the place in an absolute declarator
	 where the name was omitted).
	 For the last two cases, we have just exited the loop.

	 At this point, TYPE is the type of elements of an array,
	 or for a function to return, or for a pointer to point to.
	 After this sequence of ifs, TYPE is the type of the
	 array or function or pointer, and DECLARATOR has had its
	 outermost layer removed.  */

      if (TREE_CODE (declarator) == ARRAY_REF)
	{
	  register tree itype = NULL_TREE;
	  register tree size = TREE_OPERAND (declarator, 1);

	  /* Make sure we have a valid type for the array elements.  */
	  type = resolve_tags (type);
	  /* Make a variant that is const or volatile as needed.  */
	  if (constp || volatilep)
	    type = build_type_variant (type, constp, volatilep);
	  /* Record that any const-ness or volatility are taken care of.  */
	  constp = 0;
	  volatilep = 0;

	  /* Check for some types that there cannot be arrays of.  */

	  if (type == void_type_node)
	    {
	      yyerror ("array of voids declared");
	      type = integer_type_node;
	    }

	  if (TREE_CODE (type) == FUNCTION_TYPE)
	    {
	      yyerror ("array of functions declared");
	      type = integer_type_node;
	    }

	  /* If size was specified, set ITYPE to a range-type for that size.
	     Otherwise, ITYPE remains null.  finish_decl may figure it out
	     from an initial value.  */

	  if (size)
	    {
	      if (TREE_LITERAL (size))
		itype = make_index_type (build_int_2 (TREE_INT_CST_LOW (size) - 1, 0));
	      else
		itype = make_index_type (build_binary_op (MINUS_EXPR, size, integer_one_node));
	    }

	  /* Build the array type itself.  */

	  type = build_array_type (type, itype);

	  /* Make sure there is a valid pointer type
	     for automatic coercion of array to pointer.  */

	  layout_type (TYPE_POINTER_TO (TREE_TYPE (type)));

	  declarator = TREE_OPERAND (declarator, 0);

	  /* if size unknown, don't lay it out until finish_decl */
	  if (!itype) goto nolayout;
	}
      else if (TREE_CODE (declarator) == CALL_EXPR)
	{
	  /* Declaring a function type.
	     Make sure we have a valid type for the function to return.  */
	  type = resolve_tags (type);

	  /* Is this an error?  Should they be merged into TYPE here?  */
	  constp = 0;
	  volatilep = 0;

	  /* Warn about some types functions can't return.  */

	  if (TREE_CODE (type) == FUNCTION_TYPE)
	    {
	      yyerror ("function returning a function declared");
	      type = integer_type_node;
	    }
	  if (TREE_CODE (type) == ARRAY_TYPE)
	    {
	      yyerror ("function returning an array declared");
	      type = integer_type_node;
	    }

	  /* Construct the function type and go to the next
	     inner layer of declarator.  */

	  type =
	    build_function_type (type,
				 grokparms (TREE_OPERAND (declarator, 1)));
	  declarator = TREE_OPERAND (declarator, 0);
	}
      else if (TREE_CODE (declarator) == INDIRECT_REF)
	{
	  /* Merge any constancy or volatility into the target type
	     for the pointer.  */

	  if (constp || volatilep)
	    type = build_type_variant (type, constp, volatilep);
	  constp = 0;
	  volatilep = 0;

	  type = build_pointer_type (type);

	  /* Process a list of type modifier keywords
	     (such as const or volatile) that were given inside the `*'.  */

	  if (TREE_TYPE (declarator))
	    {
	      register tree typemodlist;
	      for (typemodlist = TREE_TYPE (declarator); typemodlist;
		   typemodlist = TREE_CHAIN (typemodlist))
		{
		  if (TREE_VALUE (typemodlist) == ridpointers[(int) RID_CONST])
		    constp = 1;
		  if (TREE_VALUE (typemodlist) == ridpointers[(int) RID_VOLATILE])
		    volatilep = 1;
		}
	    }

	  declarator = TREE_OPERAND (declarator, 0);
	}
      else
	abort ();

      layout_type (type);

      /* @@ Should perhaps replace the following code by changes in
       * @@ stor_layout.c. */
      if (TREE_CODE (type) == FUNCTION_DECL)
	{
	  /* A function variable in C should be Pmode rather than EPmode
	     because it has just the address of a function, no static chain.*/
	  TYPE_MODE (type) = Pmode;
	}

    nolayout: ;
    }

  /* Now TYPE has the actual type; but if it is an ARRAY_TYPE
     then it has not been laid out of its size wasn't specified.  */

  /* If this is declaring a typedef name, return a TYPE_DECL.  */

  if (specbits & 1 << RID_TYPEDEF)
    {
      /* Note that the grammar rejects storage classes
	 in typenames, fields or parameters */
      if (constp || volatilep)
	type = build_type_variant (type, constp, volatilep);
      return build_decl (TYPE_DECL, declarator, type, 0, 0);
    }

  /* It is ok to typedef a forward reference to a structure tag,
     but using it for other purposes requires it to be defined
     (except when it's inside a pointer; but in that case, what we
     get here would be a pointer type and `resolve_tags' would do nothing.  */

  type = resolve_tags (type);

  /* If this is a type name (such as, in a cast or sizeof),
     compute the type and return it now.  */

  if (decl_context == TYPENAME)
    {
      /* Note that the grammar rejects storage classes
	 in typenames, fields or parameters */
      if (constp || volatilep)
	type = build_type_variant (type, constp, volatilep);
      return type;
    }

  /* `void' at top level (not within pointer)
     is allowed only in typedefs or type names.  */

  if (type == void_type_node)
    {
      yyerror ("variable or field %s declared void",
	       IDENTIFIER_POINTER (declarator));
      type = integer_type_node;
    }

  /* Now create the decl, which may be a VAR_DECL, a PARM_DECL
     or a FUNCTION_DECL, depending on DECL_CONTEXT and TYPE.  */

  {
    register tree decl;

    if (decl_context == PARM)
      {
	/* A parameter declared as an array of T is really a pointer to T.
	   One declared as a function is really a pointer to a function.  */

	if (TREE_CODE (type) == ARRAY_TYPE)
	  type = build_pointer_type (TREE_TYPE (type));
	if (TREE_CODE (type) == FUNCTION_TYPE)
	  {
	    type = build_pointer_type (type);
	    layout_type (type);
	  }

	decl = build_decl (PARM_DECL, declarator, type, 0, 0);
	DECL_ARG_TYPE (decl) = type;
	if (type == float_type_node)
	  DECL_ARG_TYPE (decl) = double_type_node;
	else if (TREE_CODE (type) == INTEGER_TYPE
		 && TYPE_PRECISION (type) < TYPE_PRECISION (integer_type_node))
	  DECL_ARG_TYPE (decl) = integer_type_node;
      }
    else if (decl_context == FIELD)
      {
	/* Structure field.  It may not be a function.  */

	if (TREE_CODE (type) == FUNCTION_TYPE)
	  {
	    yyerror ("field %s declared as a function",
		     IDENTIFIER_POINTER (declarator));
	    type = build_pointer_type (type);
	    layout_type (type);
	  }
	decl = build_decl (FIELD_DECL, declarator, type, 0, 0);
      }
    else
      {
	/* Declaration in ordinary context
	   is either a variable or a function depending on TYPE.  */

	decl = build_decl (((TREE_CODE (type) == FUNCTION_TYPE)
			    ? FUNCTION_DECL : VAR_DECL),
			   declarator, type,
			   specbits & 1 << RID_STATIC,
			   specbits & 1 << RID_EXTERN);


	/* For function declaration,
	   store list of parameter names in the parameters field.
	   finish_function will replace it with
	   chained declarations of them.  */

	if (TREE_CODE (type) == FUNCTION_TYPE)
	  DECL_ARGUMENTS (decl) = last_function_parm_names;
      }

    /* Record `register' declaration for warnings on &
       and in case doing stupid register allocation.  */

    if (specbits & 1 << RID_REGISTER)
      TREE_REGDECL (decl) = 1;

    /* Record constancy and volatility.  */

    if (constp)
      TREE_READONLY (decl) = 1;
    if (volatilep)
      {
	TREE_VOLATILE (decl) = 1;
	TREE_THIS_VOLATILE (decl) = 1;
      }

    return decl;
  }
}

/* Create a type of integers to be the TYPE_DOMAIN of an ARRAY_TYPE.
   MAXVAL should be the maximum value in the domain
   (one less than the length of the array).  */

static
tree
make_index_type (maxval)
     tree maxval;
{
  register tree itype = make_node (INTEGER_TYPE);
  TYPE_PRECISION (itype) = BITS_PER_WORD;
  TYPE_MIN_VALUE (itype) = build_int_2 (0, 0);
  TREE_TYPE (TYPE_MIN_VALUE (itype)) = itype;
  TYPE_MAX_VALUE (itype) = maxval;
  TREE_TYPE (maxval) = itype;
  TYPE_MODE (itype) = SImode;
  TYPE_SIZE (itype) = TYPE_SIZE (integer_type_node);
  TYPE_SIZE_UNIT (itype) = TYPE_SIZE_UNIT (integer_type_node);
  TYPE_ALIGN (itype) = TYPE_ALIGN (integer_type_node);
  return itype;
}

/* Decode the list of parameter types for a function type.
   Given the list of things declared inside the parens,
   return a list of types.

   The list we receive can have three kinds of elements:
   an IDENTIFIER_NODE for names given without types,
   a TREE_LIST node for arguments given as typespecs or names with typespecs,
   or void_type_node, to mark the end of an argument list
   when additional arguments are not permitted (... was not used).

   If all elements of the input list contain types,
   we return a list of the types.
   If all elements contain no type (except perhaps a void_type_node
   at the end), we return a null list.
   If some have types and some do not, it is an error, and we
   return a null list.

   Also stores a list of names (IDENTIFIER_NODEs)
   in last_function_parm_names.  The list links have the names
   as the TREE_VALUE and their types (if specified) as the TREE_PURPOSE.  */

static
tree
grokparms (first_parm)
     tree first_parm;
{
  register tree parm;
  tree result = NULL_TREE;
  tree names = NULL_TREE;
  int notfirst = 0;
  int erring = 0;

  for (parm = first_parm;
       parm != NULL_TREE;
       parm = TREE_CHAIN (parm))
    {
      register tree parm_node = TREE_VALUE (parm);
      register tree name, type;
      if (TREE_CODE (parm_node) == VOID_TYPE)
	{
	  name = 0;
	  if (result != 0)
	    result = chainon (result, build_tree_list (0, parm_node));
	  break;
	}
      else if (TREE_CODE (parm_node) == IDENTIFIER_NODE)
	{
	  name = parm_node;
	  type = 0;
	}
      else
	{
	  name = TREE_VALUE (parm_node);
	  type = TREE_TYPE (grokdeclarator (TREE_VALUE (parm_node),
					    TREE_PURPOSE (parm_node),
					    PARM));
	}

      if (notfirst && !erring && (type != 0) != (result != 0))
	{
	  yyerror ("types sometimes given and sometimes omitted in parameter list");
	  erring = 1;
	}
      notfirst = 1;

      names = chainon (names, build_tree_list (type, name));
      if (type != 0)
	result = chainon (result, build_tree_list (0, type));
    }

  last_function_parm_names = names;
  if (erring)
    return 0;
  return result;
}

/* Process the specs, declarator (NULL if omitted) and width (NULL if omitted)
   of a structure component, returning a FIELD_DECL node.
   WIDTH is non-NULL for bit fields only, and is an INTEGER_CST node.

   This is done during the parsing of the struct declaration.
   The FIELD_DECL nodes are chained together and the lot of them
   are ultimately passed to `build_struct' to make the RECORD_TYPE node.  */

tree
grokfield (filename, line, declarator, declspecs, width)
     char *filename;
     int line;
     tree declarator, declspecs, width;
{
  register tree value = grokdeclarator (declarator, declspecs, FIELD);
  register tree semantics = TREE_TYPE (value);

  finish_decl (filename, line, value, NULL);
  DECL_INITIAL (value) = width;
  DECL_ALIGN (value) = TYPE_ALIGN (semantics);
  return value;
}

/* Create a RECORD_TYPE or UNION_TYPE node for a C struct or union declaration.
   CODE says which one; it is RECORD_TYPE or UNION_TYPE.
   FILENAME and LINE say where the declaration is located in the source.
   NAME is the name of the struct or union tag, or 0 if there is none.
   FIELDLIST is a chain of FIELD_DECL nodes for the fields.
   XREF is nonzero to make a cross reference to a struct or union
    defined elsewhere; this is how `struct foo' with no members
    is handled.  */

tree
build_struct (code, filename, line, name, fieldlist, xref)
     enum tree_code code;
     char *filename;
     int line;
     tree name, fieldlist;
     int xref;
{
  register tree t;
  register tree x;

  if (xref)
    {
      /* If a cross reference is requested, look up the type
	 already defined for this tag and return it.  */

      register tree ref = lookup_tag (code, name, current_binding_level, 0);
      if (ref) return ref;

      /* If no such tag is yet defined, create a forward-reference node
	 and record it as the "definition".
	 When a real declaration of this type is found,
	 the forward-reference will be altered into a real type.  */

      t = make_node (code);
      pushtag (name, t);
      return t;
    }

  /* If we have previously made forward reference to this type,
     fill in the contents in the same object that used to be the
     forward reference.  */

  if (name)
    t = lookup_tag (code, name, current_binding_level, 1);
  else
    t = 0;

  /* Otherwise, create a new node for this type.  */

  if (t == 0)
    {
      t = make_node (code);
      pushtag (name, t);
    }

  /* Install struct as DECL_CONTEXT of each field decl.
     Also process specified field sizes.
     Set DECL_SIZE_UNIT to the specified size, or 0 if none specified.
     The specified size is found in the DECL_INITIAL.
     Store 0 there, except for ": 0" fields (so we can find them
     and delete them, below).  */

  for (x = fieldlist; x; x = TREE_CHAIN (x))
    {
      DECL_CONTEXT (x) = t;
      DECL_SIZE_UNIT (x) = 0;
      /* detect invalid field size */
      if (DECL_INITIAL (x) && TREE_CODE (DECL_INITIAL (x)) != INTEGER_CST)
	{
	  yylineerror (DECL_SOURCE_LINE (x),
		       "structure field %s width not an integer constant",
		       IDENTIFIER_POINTER (DECL_NAME (x)));
	  DECL_INITIAL (x) = NULL;
	}
      /* process valid field size */
      if (DECL_INITIAL (x))
	{
	  register int prec = TREE_INT_CST_LOW (DECL_INITIAL (x));

	  if (prec == 0)
	    {
	      /* field size 0 => mark following field as "aligned" */
	      if (TREE_CHAIN (x))
		DECL_ALIGN (TREE_CHAIN (x)) = BITS_PER_WORD;
	    }
	  else
	    {
	      DECL_INITIAL (x) = NULL;
	      DECL_SIZE_UNIT (x) = prec > 0 ? prec : - prec;
	      TREE_PACKED (x) = 1;
	    }
	}
    }
  /* delete all ": 0" fields from the front of the fieldlist */
  while (fieldlist
	 && DECL_INITIAL (fieldlist))
    fieldlist = TREE_CHAIN (fieldlist);
  /* delete all such fields from the rest of the fieldlist */
  for (x = fieldlist; x;)
    {
      if (TREE_CHAIN (x) && DECL_INITIAL (TREE_CHAIN (x)))
	TREE_CHAIN (x) = TREE_CHAIN (TREE_CHAIN (x));
      else x = TREE_CHAIN (x);
    }

  TYPE_FIELDS (t) = fieldlist;
  
  layout_type (t);

  /* Round the size up to be a multiple of the required alignment */
  TYPE_SIZE (t)
    = convert_units (TYPE_SIZE (t), TYPE_SIZE_UNIT (t), TYPE_ALIGN (t));
  TYPE_SIZE_UNIT (t) = TYPE_ALIGN (t);

  return t;
}

/* Begin compiling the definition of an enumeration type.
   NAME is its name (or null if anonymous).
   Returns the type object, as yet incomplete.
   Also records info about it so that build_enumerator
   may be used to declare the individual values as they are read.  */

tree
start_enum (name)
     tree name;
{
  register tree enumtype = 0;

  /* If this is the real definition for a previous forward reference,
     fill in the contents in the same object that used to be the
     forward reference.  */

  if (name != 0)
    enumtype = lookup_tag (ENUMERAL_TYPE, name, current_binding_level, 1);

  if (enumtype == 0)
    {
      enumtype = make_node (ENUMERAL_TYPE);
      pushtag (name, enumtype);
    }

  if (TYPE_VALUES (enumtype) != 0)
    {
      /* This enum is a named one that has been declared already.  */
      yyerror ("redeclaration of enum %s", IDENTIFIER_POINTER (name));

      /* Completely replace its old definition.
	 The old enumerators remain defined, however.  */
      TYPE_VALUES (enumtype) = 0;
    }

  /* Initially, set up this enum as like `int'
     so that we can create the enumerators' declarations and values.
     Later on, the precision of the type may be changed and
     it may be layed out again.  */

  TYPE_PRECISION (enumtype) = TYPE_PRECISION (integer_type_node);
  TYPE_SIZE (enumtype) = 0;
  fixup_unsigned_type (enumtype);

  enum_next_value = 0;
  current_enum_type = enumtype;

  return enumtype;
}

/* Return the enumeration type tagged with NAME
   or create and return a forward reference to such a type.  */

tree
xref_enum (name)
     tree name;
{
  register tree ref = lookup_tag (ENUMERAL_TYPE, name, current_binding_level, 0);
  /* If we find an already-defined enum type, or a previous
     forward reference, return it.  */
  if (ref) return ref;

  /* If this is a forward reference for the first time,
     record it as the "definition".  */
  ref = make_node (ENUMERAL_TYPE);
  pushtag (name, ref);
  return ref;
}

/* After processing and defining all the values of an enumeration type,
   install their decls in the enumeration type and finish it off.
   ENUMTYPE is the type object and VALUES a list of name-value pairs.
   Returns ENUMTYPE.  */

tree
finish_enum (enumtype, values)
     register tree enumtype, values;
{
  register tree pair = values;
  register long maxvalue = 0;
  register int i;

  TYPE_VALUES (enumtype) = values;

  /* Calculate the maximum value of any enumerator in this type.  */
      
  for (pair = values; pair; pair = TREE_CHAIN (pair))
    {
      int value = TREE_INT_CST_LOW (TREE_VALUE (pair));
      if (value > maxvalue)
	maxvalue = value;
    }

#if 0
  /* Determine the precision this type needs, lay it out, and define it.  */

  for (i = maxvalue; i; i >>= 1)
    TYPE_PRECISION (enumtype)++;

  if (!TYPE_PRECISION (enumtype))
    TYPE_PRECISION (enumtype) = 1;
#endif

  /* Cancel the laying out previously done for the enum type,
     so that fixup_unsigned_type will do it over.  */
  TYPE_SIZE (enumtype) = 0;

  fixup_unsigned_type (enumtype);
  TREE_INT_CST_LOW (TYPE_MAX_VALUE (enumtype)) =  maxvalue;

  current_enum_type = 0;

  return enumtype;
}

/* Build and install a CONST_DECL for one value of the
   current enumeration type (one that was begun with start_enum).
   Return a tree-list containing the name and its value.
   Assignment of sequential values by default is handled here.  */
   
tree
build_enumerator (name, value)
     tree name, value;
{
  register tree decl;

  /* Validate and default VALUE.  */

  if (value != 0 && TREE_CODE (value) != INTEGER_CST)
    {
      yyerror ("enumerator value for %s not integer constant",
	       IDENTIFIER_POINTER (name));
      value = 0;
    }

  if (value == 0)
    value = build_int_2 (enum_next_value, 0);

  /* Set default for following value.  */

  enum_next_value = TREE_INT_CST_LOW (value) + 1;

  /* Now create a declaration for the enum value name.  */

  decl = build_decl (CONST_DECL, name,
		     current_enum_type, 0, 0);
  DECL_INITIAL (decl) = value;
  TREE_TYPE (value) = current_enum_type;
  pushdecl (decl);

  return build_tree_list (name, value);
}

/* Create the FUNCTION_DECL for a function definition.
   LINE1 is the line number that the definition absolutely begins on.
   LINE2 is the line number that the name of the function appears on.
   DECLSPECS and DECLARATOR are the parts of the declaration;
   they describe the function's name and the type it returns,
   but twisted together in a fashion that parallels the syntax of C.

   This function creates a binding context for the function body
   as well as setting up the FUNCTION_DECL in current_function_decl.

   Returns 1 on success.  If the DECLARATOR is not suitable for a function
   (it defines a datum instead), we return 0, which tells
   yyparse to report a parse error.  */

int
start_function (declspecs, declarator)
     tree declarator, declspecs;
{
  tree pushed_decl, decl1;

  current_function_returns_value = 0; /* assume it doesn't until we see it does. */

  decl1 = grokdeclarator (declarator, declspecs, NORMAL);
  if (TREE_CODE (decl1) != FUNCTION_DECL)
    return 0;

  current_function_decl = decl1;

  announce_function (current_function_decl);

  /* Make the init_value nonzero so pushdecl knows this is not tentative.
     1 is not a legitimate value,
     but it is replaced below with the LET_STMT.  */
  DECL_INITIAL (current_function_decl) = (tree)1;
  pushed_decl = pushdecl (current_function_decl);

  /* If this is an erroneous redeclaration of something not a function,
     return the original declaration (that nobody else can see)
     to avoid bombing out reading in the body of the function.  */

  if (TREE_CODE (pushed_decl) == FUNCTION_DECL)
    current_function_decl = pushed_decl;

  pushlevel ();

  make_function_rtl (current_function_decl);

  /* Allocate further tree nodes temporarily during compilation
     of this function only.  */
  temporary_allocation ();

  current_block = build_let (NULL, NULL, NULL, NULL, NULL, NULL);
  DECL_INITIAL (current_function_decl) = current_block;
  DECL_RESULT (current_function_decl)
    = build_decl (RESULT_DECL, value_identifier,
		  TREE_TYPE (TREE_TYPE (current_function_decl)), 0, 0);

  /* Make the FUNCTION_DECL's contents appear in a local tree dump
     and make the FUNCTION_DECL itself not appear in the permanent dump.  */

  TREE_PERMANENT (current_function_decl) = 0;

  /* Must mark the RESULT_DECL as being in this function.  */

  DECL_CONTEXT (DECL_RESULT (current_function_decl)) = current_block;

  return 1;
}

/* Store the parameter declarations into the current function declaration.
   This is called after parsing the parameter declarations, before
   digesting the body of the function.  */

void
store_parm_decls ()
{
  register tree parmdecls = getdecls ();
  register tree fndecl = current_function_decl;
  register tree block = DECL_INITIAL (fndecl);
  register tree parm;

  /* First match each formal parameter name with its declaration.
     The DECL_ARGUMENTS is a chain of TREE_LIST nodes
     each with a parm name as the TREE_VALUE.
     (A declared type may be in the TREE_PURPOSE.)

     Associate decls with the names and store the decls
     into the TREE_PURPOSE slots.  */

  for (parm = DECL_ARGUMENTS (fndecl); parm; parm = TREE_CHAIN (parm))
    {
      register tree tail, found = NULL;

      if (TREE_VALUE (parm) == 0)
	{
	  yylineerror (DECL_SOURCE_LINE (fndecl),
		       "parameter name missing from parameter list");
	  TREE_PURPOSE (parm) = 0;
	  continue;
	}

      /* See if any of the parmdecls specifies this parm by name.  */
      for (tail = parmdecls; tail; tail = TREE_CHAIN (tail))
	if (DECL_NAME (tail) == TREE_VALUE (parm))
	  {
	    found = tail;
	    break;
	  }

      /* If declaration already marked, we have a duplicate name.
	 Complain, and don't use this decl twice.   */
      if (found && DECL_CONTEXT (found) != 0)
	{
	  yylineerror (DECL_SOURCE_LINE (fndecl),
		       "multiple parameters named %s",
		       IDENTIFIER_POINTER (TREE_VALUE (parm)));
	  found = 0;
	}

      /* See if the type was given in the arglist.
	 If so, that overrides any parmdecl.  */
      if (TREE_PURPOSE (parm) != 0)
	{
	  /* Error to have type in the arglist and have a parmdecl.  */
	  if (found)
	    yylineerror (DECL_SOURCE_LINE (fndecl),
			 "type for parameter %s given twice",
			 IDENTIFIER_POINTER (TREE_VALUE (parm)));

	  found = build_decl (PARM_DECL, TREE_VALUE (parm),
			      TREE_PURPOSE (parm), 0, 0);
	  DECL_ARG_TYPE (found) == TREE_TYPE (found);
	  
	  DECL_SOURCE_LINE (found) = DECL_SOURCE_LINE (fndecl);
	  DECL_SOURCE_FILE (found) = DECL_SOURCE_FILE (fndecl);
	  /* Note that this pushdecl is needed despite the storedecls below
	     in order to make the name know about its decl.  */
	  pushdecl (found);
	}

      /* If no declaration found, default to int.  */
      if (!found)
	{
	  found = build_decl (PARM_DECL, TREE_VALUE (parm),
			      integer_type_node, 0, 0);
	  DECL_ARG_TYPE (found) = TREE_TYPE (found);
	  DECL_SOURCE_LINE (found) = DECL_SOURCE_LINE (fndecl);
	  DECL_SOURCE_FILE (found) = DECL_SOURCE_FILE (fndecl);
	  pushdecl (found);
	}

      TREE_PURPOSE (parm) = found;

      /* Mark this declaration as belonging to this function.  */

      DECL_CONTEXT (found) = block;
    }

  /* Complain about declarations not matched with any names.  */

  for (parm = parmdecls; parm; parm = TREE_CHAIN (parm))
    {
      if (DECL_CONTEXT (parm) == 0)
	yylineerror (DECL_SOURCE_LINE (parm),
		     "declaration for parameter %s but no such parameter",
		     IDENTIFIER_POINTER (DECL_NAME (parm)));
    }

  /* Chain the declarations together in the order of the list of names.  */
  /* Store that chain in the function decl, replacing the list of names.  */
  parm = DECL_ARGUMENTS (fndecl);
  DECL_ARGUMENTS (fndecl) = 0;
  {
    register tree last;
    for (last = 0; parm; parm = TREE_CHAIN (parm))
      if (TREE_PURPOSE (parm))
	{
	  if (last == 0)
	    DECL_ARGUMENTS (fndecl) = TREE_PURPOSE (parm);
	  else
	    TREE_CHAIN (last) = TREE_PURPOSE (parm);
	  last = TREE_PURPOSE (parm);
	  TREE_CHAIN (last) = 0;
	}
  }

  /* Now store the final chain of decls for the arguments
     as the decl-chain of the current lexical scope.  */

  storedecls (DECL_ARGUMENTS (fndecl));

  /* Compute the offset of each parameter wrt the entire arglist
     and store it in the parameter's decl node.  */

  layout_parms (fndecl);
}

/* Finish up a function declaration and compile that function
   all the way to assembler language output.  The free the storage
   for the function definition.

   This is called after parsing the body of the function definition.
   STMTS is the chain of statements that makes up the function body.  */

void
finish_function (filename, line, stmts)
     char *filename;
     int line;
     tree stmts;
{
  register tree fndecl = current_function_decl;
  register tree block = DECL_INITIAL (fndecl);
  int old_uid;

  register tree link;

/*  TREE_READONLY (fndecl) = 1;
    This caused &foo to be of type ptr-to-const-function
    which then got a warning when stored in a ptr-to-function variable.  */

  TREE_EXTERNAL (fndecl) = 0;
  TREE_STATIC (fndecl) = 1;

  finish_block (block, NULL_TREE, NULL, stmts);

  DECL_SOURCE_FILE (block) = filename;
  DECL_SOURCE_LINE (block) = line;

  poplevel ();
  current_switch_stmt = NULL;
  current_block = NULL;

  rest_of_compilation (fndecl, 1);

  /* Free all the tree nodes making up this function.  */
  /* Switch back to allocating nodes permanently
     until we start another function.  */
  permanent_allocation ();

  /* Stop pointing to the local nodes about to be freed.  */
  /* But DECL_INITIAL must remain nonzero so we know this
     was an actual function definition.  */
  DECL_INITIAL (fndecl) = (tree) 1;
  DECL_ARGUMENTS (fndecl) = 0;
  DECL_RESULT (fndecl) = 0;
}

tree
implicitly_declare (functionid)
     tree functionid;
{
  register tree decl
    = build_decl (FUNCTION_DECL, functionid,
		  default_function_type, 0, 1);

  /* ANSI standard says implicit declarations are in the innermost block */
  pushdecl (decl);
  return decl;
}

/* Return nonzero if the declaration new is legal
   when the declaration old (assumed to be for the same name)
   has already been seen.  */

int
allowed_redeclaration (new, old, global)
     tree new, old;
     int global;
{
  if (!comptypes (TREE_TYPE (new), TREE_TYPE (old)))
    return 0;

  if (global)
    {
      /* Reject two definitions.  */
      if (DECL_INITIAL (old) != 0 && DECL_INITIAL (new) != 0)
	return 0;
      /* Reject two tentative definitions with different linkage.  */
      if (!TREE_EXTERNAL (new) && !TREE_EXTERNAL (old)
	  && TREE_STATIC (old) != TREE_STATIC (new))
	return 0;
      return 1;
    }
  else if (TREE_CODE (TREE_TYPE (new)) == FUNCTION_TYPE)
    {
      /* Declarations of functions inside blocks
	 are just references, and do not determine linkage.  */
      return 1;
    }
  else
    {
      /* Reject two definitions, and reject a definition
	 together with an external reference.  */
      return TREE_EXTERNAL (new) && TREE_EXTERNAL (old);
    }
}
