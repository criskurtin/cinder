from __future__ import annotations

import ast
from ast import AST
from types import CodeType
from typing import Optional, Type, cast

from ..opcodes import opcode
from ..pyassem import PyFlowGraph, PyFlowGraphCinder
from ..pycodegen import (
    CodeGenerator,
    CinderCodeGenerator,
    FuncOrLambda,
    CompNode,
)
from ..symbols import (
    ClassScope,
    FunctionScope,
    ModuleScope,
    Scope,
    CinderSymbolVisitor,
    SymbolVisitor,
)
from .type_binder import ReadonlyTypeBinder, TReadonlyTypes
from .types import READONLY, FunctionValue
from .util import calc_function_readonly_mask


class ReadonlyCodeGenerator(CinderCodeGenerator):
    flow_graph = PyFlowGraphCinder
    _SymbolVisitor = CinderSymbolVisitor

    def __init__(
        self,
        parent: Optional[CodeGenerator],
        node: AST,
        symbols: SymbolVisitor,
        graph: PyFlowGraph,
        binder: ReadonlyTypeBinder,
        flags: int = 0,
        optimization_lvl: int = 0,
    ) -> None:
        super().__init__(
            parent, node, symbols, graph, flags=flags, optimization_lvl=optimization_lvl
        )
        self.binder = binder

    @classmethod
    def make_code_gen(
        cls,
        module_name: str,
        tree: AST,
        filename: str,
        flags: int,
        optimize: int,
        peephole_enabled: bool = True,
        ast_optimizer_enabled: bool = True,
    ) -> ReadonlyCodeGenerator:
        s = cls._SymbolVisitor()
        s.visit(tree)
        binder = ReadonlyTypeBinder(tree, filename, s)
        graph = cls.flow_graph(
            module_name,
            filename,
            s.scopes[tree],
            peephole_enabled=peephole_enabled,
        )
        codegen = cls(
            None,
            tree,
            s,
            graph,
            binder,
            flags=flags,
            optimization_lvl=optimize,
        )
        codegen.visit(tree)
        return codegen

    def make_child_codegen(
        self,
        tree: FuncOrLambda | CompNode | ast.ClassDef,
        graph: PyFlowGraph,
        codegen_type: Optional[Type[CodeGenerator]] = None,
    ) -> CodeGenerator:
        if codegen_type is None:
            codegen_type = type(self)
        assert issubclass(codegen_type, ReadonlyCodeGenerator)
        codegen_type = cast(Type[ReadonlyCodeGenerator], codegen_type)
        return codegen_type(
            self,
            tree,
            self.symbols,
            graph,
            binder=self.binder,
            flags=self.flags,
            optimization_lvl=self.optimization_lvl,
        )

    def visitName(self, node: ast.Name) -> None:
        if node.id != "__function_credential__":
            super().visitName(node)
            return

        module_name = ""
        class_name = ""
        func_name = ""
        scope = self.scope
        names = []
        collecting_function_name = True

        while scope and not isinstance(scope, ModuleScope):
            if isinstance(scope, ClassScope) and collecting_function_name:
                func_name = ".".join(reversed(names))
                collecting_function_name = False
                names = [scope.name]
            else:
                names.append(scope.name)
            scope = scope.parent

        if collecting_function_name:
            func_name = ".".join(reversed(names))
        else:
            class_name = ".".join(reversed(names))

        if scope:
            assert isinstance(scope, ModuleScope)
            module_name = scope.name

        name_tuple = (module_name, class_name, func_name)
        self.emit("FUNC_CREDENTIAL", name_tuple)

    def emit_readonly_op(self, opname: str, arg: object) -> None:
        op = opcode.readonlyop[opname]
        self.emit("READONLY_OPERATION", (op, arg))

    def build_function(
        self,
        node: ast.FunctionDef | ast.AsyncFunctionDef | ast.Lambda,
        gen: CodeGenerator,
    ) -> None:
        super().build_function(node, gen)
        readonly_funcs = self.binder.read_only_funcs

        if node not in readonly_funcs:
            return

        func_value = readonly_funcs[node]
        assert isinstance(func_value, FunctionValue)

        func_value_tuple = (
            func_value.returns_readonly,
            func_value.readonly_nonlocal,
            tuple(x == READONLY for x in func_value.args),
        )

        mask = calc_function_readonly_mask(func_value_tuple)
        self.emit_readonly_op("MAKE_FUNCTION", mask)

def readonly_compile(
    name: str, filename: str, tree: AST, flags: int, optimize: int
) -> CodeType:
    """
    Entry point used in non-static setting
    """
    codegen = ReadonlyCodeGenerator.make_code_gen(name, tree, filename, flags, optimize)
    return codegen.getCode()