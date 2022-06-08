/**
 * Find functions not annotated with __init (or similar) which call
 * __init-annotated functions. These instances are potential bugs.
 *
 * Examples:
 *  - https://stackoverflow.com/a/70823863/3889449
 *  - https://git.kernel.org/pub/scm/linux/kernel/git/torvalds/linux.git/commit/?id=45967ffb9e50aa3472cc6c69a769ef0f09cced5d
 *  - https://git.kernel.org/pub/scm/linux/kernel/git/torvalds/linux.git/commit/?id=5f117033243488a0080f837540c27999aa31870e
 */

import cpp
import utils

from KernelFunc func, KernelFunc caller, FunctionCall call
where
	(func.isInSection(".init.text") or func.isInSection(".head.text"))
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
