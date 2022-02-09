/**
 * Find functions not annotated with __init (or similar) which call
 * __init-annotated functions. These instances are potential bugs.
 * Example: https://stackoverflow.com/a/70823863/3889449
 */

import cpp
import utils

from KernelFunc func, KernelFunc caller, FunctionCall call
where
	func.isInSection(".init.text")
	and call = func.getACallToThisFunction()
	and caller = call.getEnclosingFunction()
	and not caller.isInline()
	and not caller.isInSection(".init.text")
	and not caller.isInSection(".ref.text")
	and not caller.isInSection(".head.text")
select
	func as InitFunc,
	caller as Caller,
	call.getEnclosingStmt() as Statement,
	call.getLocation() as Location
