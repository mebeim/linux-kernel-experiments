/**
 * Find out all structs containing function pointers that are ever kfree'd at
 * least once (and where), plus their corresponding kmalloc slab size. Useful
 * for leaking KASLR.
 */

import cpp
import utils

FunctionPointerType anyFuncPtr(Struct s) {
	result = s.getAField().getUnspecifiedType()
	or result = anyFuncPtr(s.getAField().getUnspecifiedType())
}

from FunctionCall f, PointerType p, FunctionPointerType fp, Struct s
where
	f.getTarget().hasName("kfree")
	and p = f.getArgument(0).getType()
	and s = p.getBaseType().getUnspecifiedType()
	and fp = anyFuncPtr(s)
select s.getDefinitionLocation(), s as struct, s.getSize() as size, kmallocSlab(s.getSize()) as slab
