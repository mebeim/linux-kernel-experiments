/**
 * Find out all structs that are ever kfree'd at least once (and where) and
 * their corresponding kmalloc slab size. Useful for use-after-free scenarios.
 */

import cpp
import utils

// TODO: refine this, I don't think it caches everything...

from
	FunctionCall f,
	AssignExpr a,
	Initializer i,
	DeclarationEntry d,
	PointerType p,
	Struct s
where
	f.getTarget().hasName("kmalloc")
	and (
		i = f.getParent() and d = i.getDeclaration().getADeclarationEntry() and p = d.getType()
		or a = f.getParent() and p = a.getType()
	)
	and s = p.getBaseType().getUnspecifiedType()
select f.getLocation() as location, s as struct, s.getSize() as size, kmallocSlab(s.getSize()) as slab
