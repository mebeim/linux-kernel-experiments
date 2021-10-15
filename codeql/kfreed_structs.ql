/**
 * Find out all structs that are ever kfree'd at least once (and where) and
 * their corresponding kmalloc slab size. Useful for data leaks.
 */

import cpp
import utils

from FunctionCall f, PointerType p, Struct s
where
	f.getTarget().hasName("kfree")
	and p = f.getArgument(0).getType()
	and s = p.getBaseType().getUnspecifiedType()
select
	f.getLocation() as location, s as struct, s.getSize() as size, kmallocSlab(s.getSize()) as slab
