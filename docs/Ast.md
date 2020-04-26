
AstNodeScript
- classes : [AstNodeClass]
- functions : [AstNodeFunction]
- mainFunction : AstNodeFunction

AstNodeFunction
- name : string
- arguments : AstNodeDeclList
- body : AstNodeBlock

AstNodeDeclList
- decls: [pair<string, AstNodeTypeName>]

AstNodeBlock (AstNodeStatement)
- statements : [AstNodeStatement]

AstNodeStatement
- type : {assert, block, expression, for, forRange, if, return, switch, throw, tryCatch}

AstNodeExpression
- type : {assignment, binaryExpr, call, function, identifier, indexed, list, literal, member, property, unaryExpr}

AstNodeAssignment (AstNodeExpression)
- target : AstNodeExpression
- expression : AstNodeExpression

AstNodeBinaryExpr (AstNodeExpression)
- type : {add, subtract,
          divide, module, multiply,
          and_, or_,
          equals, greater, greaterEq, less, lessEq, notEquals}
- left : AstNodeExpression
- right : AstNodeExpression

AstNodeUnaryExpr (AstNodeExpression)
- type : {negation, not_}
- right : AstNodeExpression

AstNodeList (AstNodeExpression)
- item : [AstNodeExpression]

AstNodeLiteral
- type : {boolean, integer, nul, object, real, string}

AstNodeLiteralObject (AstNodeLiteral)
- properties : [pair<string, AstNodeExpression>]
