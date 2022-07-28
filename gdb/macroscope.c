/* Functions for deciding which macros are currently in scope.
   Copyright (C) 2002-2022 Free Software Foundation, Inc.
   Contributed by Red Hat, Inc.

   This file is part of GDB.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 3 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.  */

#include "defs.h"

#include "macroscope.h"
#include "symtab.h"
#include "source.h"
#include "target.h"
#include "frame.h"
#include "inferior.h"
#include "complaints.h"

/* A table of user-defined macros.  Unlike the macro tables used for
   symtabs, this one uses xmalloc for all its allocation, not an
   obstack, and it doesn't bcache anything; it just xmallocs things.  So
   it's perfectly possible to remove things from this, or redefine
   things.  */
struct macro_table *macro_user_macros;


gdb::unique_xmalloc_ptr<struct macro_scope>
sal_macro_scope (struct symtab_and_line sal)
{
  struct macro_source_file *main_file, *inclusion;
  struct compunit_symtab *cust;

  if (sal.symtab == NULL)
    return NULL;

  cust = sal.symtab->compunit ();
  if (cust->macro_table () == NULL)
    return NULL;

  gdb::unique_xmalloc_ptr<struct macro_scope> ms (XNEW (struct macro_scope));

  main_file = macro_main (cust->macro_table ());
  inclusion = macro_lookup_inclusion (main_file, sal.symtab->filename_for_id);

  if (inclusion)
    {
      ms->file = inclusion;
      ms->line = sal.line;
    }
  else
    {
      /* There are, unfortunately, cases where a compilation unit can
	 have a symtab for a source file that doesn't appear in the
	 macro table.  For example, at the moment, Dwarf doesn't have
	 any way in the .debug_macinfo section to describe the effect
	 of #line directives, so if you debug a YACC parser you'll get
	 a macro table which only mentions the .c files generated by
	 YACC, but symtabs that mention the .y files consumed by YACC.

	 In the long run, we should extend the Dwarf macro info
	 representation to handle #line directives, and get GCC to
	 emit it.

	 For the time being, though, we'll just treat these as
	 occurring at the end of the main source file.  */
      ms->file = main_file;
      ms->line = -1;

      complaint (_("symtab found for `%s', but that file\n"
		 "is not covered in the compilation unit's macro information"),
		 symtab_to_filename_for_display (sal.symtab));
    }

  return ms;
}


gdb::unique_xmalloc_ptr<struct macro_scope>
user_macro_scope (void)
{
  gdb::unique_xmalloc_ptr<struct macro_scope> ms (XNEW (struct macro_scope));
  ms->file = macro_main (macro_user_macros);
  ms->line = -1;
  return ms;
}

gdb::unique_xmalloc_ptr<struct macro_scope>
default_macro_scope (void)
{
  struct symtab_and_line sal;
  gdb::unique_xmalloc_ptr<struct macro_scope> ms;
  struct frame_info *frame;
  CORE_ADDR pc;

  /* If there's a selected frame, use its PC.  */
  frame = deprecated_safe_get_selected_frame ();
  if (frame && get_frame_pc_if_available (frame, &pc))
    sal = find_pc_line (pc, 0);

  /* Fall back to the current listing position.  */
  else
    {
      /* Don't call select_source_symtab here.  That can raise an
	 error if symbols aren't loaded, but GDB calls the expression
	 evaluator in all sorts of contexts.

	 For example, commands like `set width' call the expression
	 evaluator to evaluate their numeric arguments.  If the
	 current language is C, then that may call this function to
	 choose a scope for macro expansion.  If you don't have any
	 symbol files loaded, then get_current_or_default would raise an
	 error.  But `set width' shouldn't raise an error just because
	 it can't decide which scope to macro-expand its argument in.  */
      struct symtab_and_line cursal
	= get_current_source_symtab_and_line ();
      
      sal.symtab = cursal.symtab;
      sal.line = cursal.line;
    }

  ms = sal_macro_scope (sal);
  if (! ms)
    ms = user_macro_scope ();

  return ms;
}


/* Look up the definition of the macro named NAME in scope at the source
   location given by BATON, which must be a pointer to a `struct
   macro_scope' structure.  */
struct macro_definition *
standard_macro_lookup (const char *name, const macro_scope &ms)
{
  /* Give user-defined macros priority over all others.  */
  macro_definition *result
    = macro_lookup_definition (macro_main (macro_user_macros), -1, name);

  if (result == nullptr)
    result = macro_lookup_definition (ms.file, ms.line, name);

  return result;
}

void _initialize_macroscope ();
void
_initialize_macroscope ()
{
  macro_user_macros = new_macro_table (NULL, NULL, NULL);
  macro_set_main (macro_user_macros, "<user-defined>");
  macro_allow_redefinitions (macro_user_macros);
}
