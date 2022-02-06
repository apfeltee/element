
items=(
GetDefaultModule=getDefaultModule
GetModuleForFile=getModuleForFile
NewString=makeString
NewArray=makeArray
NewObject=makeObject
NewFunction=makeFunction
NewCoroutine=makeCoroutine
NewBox=makeBox
NewIterator=makeIterator
NewError=makeError
NewRootExecutionContext=makeRootExecutionContext
GarbageCollect=collectGarbage
UpdateGcRelationship=updateGCRelationship
GetHeapObjectsCount=heapObjectsCount
DeleteHeap=deleteHeap
AddToHeap=addToHeap
FreeGC=freeGC
MakeGrayIfNeeded=makeGrayIfNeeded
MarkRoots=markRoots
Mark=mark
SweepHead=sweepHead
SweepRest=sweepRest
LoadMemberFromObject=loadMemberFromObject
RegisterStandardUtilities=registerBuiltins
LocationFromFrame=locationFromFrame
nativefunctions=Builtins
ResolveNamesInNodes=resolveNamesInNodes
TryToFindNameInTheEnclosingFunctions=tryFindNameInEnclosing
AddNativeFunction=addNative
CurrentExpressionType=currentExprType
FoldOperatorStacks=foldOperStacks
IsExpressionTerminator=isExprTerminator
ParseExpression=parseExpr
ParsePrimary=parsePrimary
ParsePrimitive=parsePrimitive
ParseVarialbe=parseVariable
parseParenthesis=parseParenthesis
ParseIndexOperator=parseIndexOper
ParseBlock=parseBlockStmt
ParseFunction=parseFunction
ParseArrayOrObject=parseArrayOrObject
ParseIf=parseIfStmt
ParseWhile=parseWhileStmt
ParseFor=parseForStmt
ParseControlExpression=parseControlExpr
Parse=parse
HandleCommentOrDivision=handleCommentOrDiv
HandleSingleChar=handleSingleChar
HandleSubstractOrArrow=handleSubOrArray
GetLastArgumentIndex=getLastArgIndex
RewindDueToMissingElse=rewindBecauseMissingElse
GetHashFromName=hashFromName
)

gsub -r  -f element.h -f ast.cpp -f compiler.cpp -f constant.cpp -f datatypes.cpp -f filemanager.cpp -f gc.cpp -f lexer.cpp -f logger.cpp -f main.cpp -f memory.cpp -f native.cpp -f opcodes.cpp -f operators.cpp -f parser.cpp -f semantic.cpp -f symbol.cpp -f tokens.cpp -f value.cpp -f vm.cpp -b "${items[@]}"

