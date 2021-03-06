//===--- IndexSymbol.cpp - Types and functions for indexing symbols -------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#include "clang/Index/IndexSymbol.h"
#include "clang/AST/DeclCXX.h"
#include "clang/AST/DeclObjC.h"
#include "clang/AST/DeclTemplate.h"
#include "clang/AST/PrettyPrinter.h"

using namespace clang;
using namespace clang::index;

SymbolInfo index::getSymbolInfo(const Decl *D) {
  assert(D);
  SymbolInfo Info;
  Info.Kind = SymbolKind::Unknown;
  Info.TemplateKind = SymbolCXXTemplateKind::NonTemplate;
  Info.Lang = SymbolLanguage::C;

  if (const TagDecl *TD = dyn_cast<TagDecl>(D)) {
    switch (TD->getTagKind()) {
    case TTK_Struct:
      Info.Kind = SymbolKind::Struct; break;
    case TTK_Union:
      Info.Kind = SymbolKind::Union; break;
    case TTK_Class:
      Info.Kind = SymbolKind::Class;
      Info.Lang = SymbolLanguage::CXX;
      break;
    case TTK_Interface:
      Info.Kind = SymbolKind::Protocol;
      Info.Lang = SymbolLanguage::CXX;
      break;
    case TTK_Enum:
      Info.Kind = SymbolKind::Enum; break;
    }

    if (const CXXRecordDecl *CXXRec = dyn_cast<CXXRecordDecl>(D))
      if (!CXXRec->isCLike())
        Info.Lang = SymbolLanguage::CXX;

    if (isa<ClassTemplatePartialSpecializationDecl>(D)) {
      Info.TemplateKind = SymbolCXXTemplateKind::TemplatePartialSpecialization;
    } else if (isa<ClassTemplateSpecializationDecl>(D)) {
      Info.TemplateKind = SymbolCXXTemplateKind::TemplateSpecialization;
    }

  } else {
    switch (D->getKind()) {
    case Decl::Import:
      Info.Kind = SymbolKind::Module;
      break;
    case Decl::Typedef:
      Info.Kind = SymbolKind::TypeAlias; break; // Lang = C
    case Decl::Function:
      Info.Kind = SymbolKind::Function;
      break;
    case Decl::ParmVar:
      Info.Kind = SymbolKind::Variable;
      break;
    case Decl::Var:
      Info.Kind = SymbolKind::Variable;
      if (isa<CXXRecordDecl>(D->getDeclContext())) {
        Info.Kind = SymbolKind::StaticProperty;
        Info.Lang = SymbolLanguage::CXX;
      }
      break;
    case Decl::Field:
      Info.Kind = SymbolKind::Field;
      if (const CXXRecordDecl *
            CXXRec = dyn_cast<CXXRecordDecl>(D->getDeclContext())) {
        if (!CXXRec->isCLike())
          Info.Lang = SymbolLanguage::CXX;
      }
      break;
    case Decl::EnumConstant:
      Info.Kind = SymbolKind::EnumConstant; break;
    case Decl::ObjCInterface:
    case Decl::ObjCImplementation:
      Info.Kind = SymbolKind::Class;
      Info.Lang = SymbolLanguage::ObjC;
      break;
    case Decl::ObjCProtocol:
      Info.Kind = SymbolKind::Protocol;
      Info.Lang = SymbolLanguage::ObjC;
      break;
    case Decl::ObjCCategory:
    case Decl::ObjCCategoryImpl:
      Info.Kind = SymbolKind::Extension;
      Info.Lang = SymbolLanguage::ObjC;
      break;
    case Decl::ObjCMethod:
      if (cast<ObjCMethodDecl>(D)->isInstanceMethod())
        Info.Kind = SymbolKind::InstanceMethod;
      else
        Info.Kind = SymbolKind::ClassMethod;
      Info.Lang = SymbolLanguage::ObjC;
      break;
    case Decl::ObjCProperty:
      Info.Kind = SymbolKind::InstanceProperty;
      Info.Lang = SymbolLanguage::ObjC;
      break;
    case Decl::ObjCIvar:
      Info.Kind = SymbolKind::Field;
      Info.Lang = SymbolLanguage::ObjC;
      break;
    case Decl::Namespace:
      Info.Kind = SymbolKind::Namespace;
      Info.Lang = SymbolLanguage::CXX;
      break;
    case Decl::NamespaceAlias:
      Info.Kind = SymbolKind::NamespaceAlias;
      Info.Lang = SymbolLanguage::CXX;
      break;
    case Decl::CXXConstructor:
      Info.Kind = SymbolKind::Constructor;
      Info.Lang = SymbolLanguage::CXX;
      break;
    case Decl::CXXDestructor:
      Info.Kind = SymbolKind::Destructor;
      Info.Lang = SymbolLanguage::CXX;
      break;
    case Decl::CXXConversion:
      Info.Kind = SymbolKind::ConversionFunction;
      Info.Lang = SymbolLanguage::CXX;
      break;
    case Decl::CXXMethod: {
      const CXXMethodDecl *MD = cast<CXXMethodDecl>(D);
      if (MD->isStatic())
        Info.Kind = SymbolKind::StaticMethod;
      else
        Info.Kind = SymbolKind::InstanceMethod;
      Info.Lang = SymbolLanguage::CXX;
      break;
    }
    case Decl::ClassTemplate:
      Info.Kind = SymbolKind::Class;
      Info.TemplateKind = SymbolCXXTemplateKind::Template;
      Info.Lang = SymbolLanguage::CXX;
      break;
    case Decl::FunctionTemplate:
      Info.Kind = SymbolKind::Function;
      Info.TemplateKind = SymbolCXXTemplateKind::Template;
      Info.Lang = SymbolLanguage::CXX;
      if (const CXXMethodDecl *MD = dyn_cast_or_null<CXXMethodDecl>(
                           cast<FunctionTemplateDecl>(D)->getTemplatedDecl())) {
        if (isa<CXXConstructorDecl>(MD))
          Info.Kind = SymbolKind::Constructor;
        else if (isa<CXXDestructorDecl>(MD))
          Info.Kind = SymbolKind::Destructor;
        else if (isa<CXXConversionDecl>(MD))
          Info.Kind = SymbolKind::ConversionFunction;
        else {
          if (MD->isStatic())
            Info.Kind = SymbolKind::StaticMethod;
          else
            Info.Kind = SymbolKind::InstanceMethod;
        }
      }
      break;
    case Decl::TypeAliasTemplate:
      Info.Kind = SymbolKind::TypeAlias;
      Info.Lang = SymbolLanguage::CXX;
      Info.TemplateKind = SymbolCXXTemplateKind::Template;
      break;
    case Decl::TypeAlias:
      Info.Kind = SymbolKind::TypeAlias;
      Info.Lang = SymbolLanguage::CXX;
      break;
    default:
      break;
    }
  }

  if (Info.Kind == SymbolKind::Unknown)
    return Info;

  if (const FunctionDecl *FD = dyn_cast<FunctionDecl>(D)) {
    if (FD->getTemplatedKind() ==
          FunctionDecl::TK_FunctionTemplateSpecialization)
      Info.TemplateKind = SymbolCXXTemplateKind::TemplateSpecialization;
  }

  if (Info.TemplateKind != SymbolCXXTemplateKind::NonTemplate)
    Info.Lang = SymbolLanguage::CXX;

  return Info;
}

void index::applyForEachSymbolRole(SymbolRoleSet Roles,
                                   llvm::function_ref<void(SymbolRole)> Fn) {
#define APPLY_FOR_ROLE(Role) \
  if (Roles & (unsigned)SymbolRole::Role) \
    Fn(SymbolRole::Role)

  APPLY_FOR_ROLE(Declaration);
  APPLY_FOR_ROLE(Definition);
  APPLY_FOR_ROLE(Reference);
  APPLY_FOR_ROLE(Read);
  APPLY_FOR_ROLE(Write);
  APPLY_FOR_ROLE(Call);
  APPLY_FOR_ROLE(Dynamic);
  APPLY_FOR_ROLE(AddressOf);
  APPLY_FOR_ROLE(Implicit);
  APPLY_FOR_ROLE(RelationChildOf);
  APPLY_FOR_ROLE(RelationBaseOf);
  APPLY_FOR_ROLE(RelationOverrideOf);
  APPLY_FOR_ROLE(RelationReceivedBy);
  APPLY_FOR_ROLE(RelationCalledBy);

#undef APPLY_FOR_ROLE
}

void index::printSymbolRoles(SymbolRoleSet Roles, raw_ostream &OS) {
  bool VisitedOnce = false;
  applyForEachSymbolRole(Roles, [&](SymbolRole Role) {
    if (VisitedOnce)
      OS << ',';
    else
      VisitedOnce = true;
    switch (Role) {
    case SymbolRole::Declaration: OS << "Decl"; break;
    case SymbolRole::Definition: OS << "Def"; break;
    case SymbolRole::Reference: OS << "Ref"; break;
    case SymbolRole::Read: OS << "Read"; break;
    case SymbolRole::Write: OS << "Writ"; break;
    case SymbolRole::Call: OS << "Call"; break;
    case SymbolRole::Dynamic: OS << "Dyn"; break;
    case SymbolRole::AddressOf: OS << "Addr"; break;
    case SymbolRole::Implicit: OS << "Impl"; break;
    case SymbolRole::RelationChildOf: OS << "RelChild"; break;
    case SymbolRole::RelationBaseOf: OS << "RelBase"; break;
    case SymbolRole::RelationOverrideOf: OS << "RelOver"; break;
    case SymbolRole::RelationReceivedBy: OS << "RelRec"; break;
    case SymbolRole::RelationCalledBy: OS << "RelCall"; break;
    }
  });
}

bool index::printSymbolName(const Decl *D, const LangOptions &LO,
                            raw_ostream &OS) {
  if (auto *ND = dyn_cast<NamedDecl>(D)) {
    PrintingPolicy Policy(LO);
    // Forward references can have different template argument names. Suppress
    // the template argument names in constructors to make their name more
    // stable.
    Policy.SuppressTemplateArgsInCXXConstructors = true;
    DeclarationName DeclName = ND->getDeclName();
    if (DeclName.isEmpty())
      return true;
    DeclName.print(OS, Policy);
    return false;
  } else {
    return true;
  }
}

StringRef index::getSymbolKindString(SymbolKind K) {
  switch (K) {
  case SymbolKind::Unknown: return "<unknown>";
  case SymbolKind::Module: return "module";
  case SymbolKind::Namespace: return "namespace";
  case SymbolKind::NamespaceAlias: return "namespace-alias";
  case SymbolKind::Macro: return "macro";
  case SymbolKind::Enum: return "enum";
  case SymbolKind::Struct: return "struct";
  case SymbolKind::Class: return "class";
  case SymbolKind::Protocol: return "protocol";
  case SymbolKind::Extension: return "extension";
  case SymbolKind::Union: return "union";
  case SymbolKind::TypeAlias: return "type-alias";
  case SymbolKind::Function: return "function";
  case SymbolKind::Variable: return "variable";
  case SymbolKind::Field: return "field";
  case SymbolKind::EnumConstant: return "enumerator";
  case SymbolKind::InstanceMethod: return "instance-method";
  case SymbolKind::ClassMethod: return "class-method";
  case SymbolKind::StaticMethod: return "static-method";
  case SymbolKind::InstanceProperty: return "instance-property";
  case SymbolKind::ClassProperty: return "class-property";
  case SymbolKind::StaticProperty: return "static-property";
  case SymbolKind::Constructor: return "constructor";
  case SymbolKind::Destructor: return "destructor";
  case SymbolKind::ConversionFunction: return "coversion-func";
  }
}

StringRef index::getTemplateKindStr(SymbolCXXTemplateKind TK) {
  switch (TK) {
  case SymbolCXXTemplateKind::NonTemplate: return "NT";
  case SymbolCXXTemplateKind::Template : return "T";
  case SymbolCXXTemplateKind::TemplatePartialSpecialization : return "TPS";
  case SymbolCXXTemplateKind::TemplateSpecialization: return "TS";
  }
}

StringRef index::getSymbolLanguageString(SymbolLanguage K) {
  switch (K) {
  case SymbolLanguage::C: return "C";
  case SymbolLanguage::ObjC: return "ObjC";
  case SymbolLanguage::CXX: return "C++";
  }
}
