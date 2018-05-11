#include "aotjs_runtime.h"

#include <sstream>

namespace AotJS {
  Typeof typeof_undefined = "undefined";
  Typeof typeof_number = "number";
  Typeof typeof_boolean = "boolean";
  Typeof typeof_string = "string";
  Typeof typeof_symbol = "symbol";
  Typeof typeof_function = "function";
  Typeof typeof_object = "object";
}

#pragma mark Val hash helpers

namespace std {
  size_t hash<::AotJS::Val>::operator()(::AotJS::Val const& ref) const noexcept {
    return hash<uint64_t>{}(ref.raw());
  }
}

namespace AotJS {
  bool Val::operator==(const Val &rhs) const {
    // Bit-identical always matches!
    // This catches matching pointers (same object), matching ints, etc.
    if (raw() == rhs.raw()) {
      return true;
    }

    if (isString() && rhs.isString()) {
      // Two string instances may still compare equal.
      return asString() == rhs.asString();
    }

    // Non-identical non-string objects never compare identical.
    return false;
  }

  string Val::dump() const {
    std::ostringstream buf;
    if (isDouble()) {
      buf << asDouble();
    } else if (isInt32()) {
      buf << asInt32();
    } else if (isBool()) {
      if (asBool()) {
        buf << "true";
      } else {
        buf << "false";
      }
    } else if (isNull()) {
      buf << "null";
    } else if (isUndefined()) {
      buf << "undefined";
    } else if (isJSThing()) {
      buf << asJSThing()->dump();
    }
    return buf.str();
  }

  #pragma mark GCThing


  GCThing::~GCThing() {
    //
  }

  void GCThing::markRefsForGC() {
    // no-op default
  }

  string GCThing::dump() {
    return "GCThing";
  }

  #pragma mark JSThing

  JSThing::~JSThing() {
    //
  }

  Typeof JSThing::typeof() const {
    return "invalid-jsthing";
  }

  string JSThing::dump() {
    return "JSThing";
  }

  #pragma mark PropIndex
  PropIndex::~PropIndex() {
    //
  }

  #pragma mark Object

  Object::~Object() {
    //
  }

  Typeof Object::typeof() const {
    return typeof_object;
  }

  static Val normalizePropName(Val aName) {
    if (aName.isString()) {
      return aName;
    } else if (aName.isSymbol()) {
      return aName;
    } else {
      // todo: convert to string
      // hrm, we need a ref to an engine for that?
      std::abort();
      return Undefined();
    }
  }

  // todo: handle numeric indices
  // todo: getters
  Val Object::getProp(Val aName) {
    auto name = normalizePropName(aName);
    auto index = mProps.find(name);
    if (index == mProps.end()) {
      if (mPrototype) {
        return mPrototype->getProp(name);
      } else {
        return Undefined();
      }
    } else {
      return index->first;
    }
  }

  void Object::setProp(Val aName, Val aVal) {
    auto name = normalizePropName(aName);
    mProps.emplace(name, aVal);
  }

  void Object::markRefsForGC() {
    for (auto iter : mProps) {
      auto prop_name(iter.first);
      auto prop_val(iter.second);

      // prop names are always either strings or symbols,
      // so they are pointers to GCThings.
      prop_name.asJSThing()->markForGC();

      // prop values may not be, so check!
      if (prop_val.isJSThing()) {
        prop_val.asJSThing()->markForGC();
      }
    }
  }

  string Object::dump() {
    std::ostringstream buf;
    buf << "Object({";

    bool first = true;
    for (auto iter : mProps) {
      auto name = iter.first;
      auto val = iter.second;

      if (first) {
        first = false;
      } else {
        buf << ",";
      }
      buf << name.dump();
      buf << ":";
      buf << val.dump();
    }
    buf << "})";
    return buf.str();
  }

  #pragma mark String

  String::~String() {
    //
  }

  Typeof String::typeof() const {
    return typeof_string;
  }

  string String::dump() {
    // todo: emit JSON or something
    std::ostringstream buf;
    buf << "\"";
    buf << data;
    buf << "\"";
    return buf.str();
  }

  #pragma mark Symbol

  Symbol::~Symbol() {
    //
  }

  Typeof Symbol::typeof() const {
    return typeof_symbol;
  }

  string Symbol::dump() {
    std::ostringstream buf;
    buf << "Symbol(\"";
    buf << name;
    buf << "\")";
    return buf.str();
  }

  #pragma mark Scope

  Scope::~Scope() {
    //
  }

  void Scope::markRefsForGC() {
    if (mParent) {
      mParent->markRefsForGC();
    }

    for (auto local : mLocals) {
      if (local.isJSThing()) {
        local.asJSThing()->markRefsForGC();
      }
    }
  }

  string Scope::dump() {
    std::ostringstream buf;
    buf << "Scope(...)";
    return buf.str();
  }

  #pragma mark Frame

  Frame::~Frame() {
    //
  }

  void Frame::markRefsForGC() {
    if (mParent) {
      mParent->markRefsForGC();
    }

    mFunc->markRefsForGC();

    if (mThis.isJSThing()) {
      mThis.asJSThing()->markRefsForGC();
    }

    for (auto val : mArgs) {
      if (val.isJSThing()) {
        val.asJSThing()->markRefsForGC();
      }
    }
  }

  string Frame::dump() {
    std::ostringstream buf;
    buf << "Frame(...)";
    return buf.str();
  }

  #pragma mark Function

  Function::~Function() {
    //
  }

  Typeof Function::typeof() const {
    return typeof_function;
  }

  void Function::markRefsForGC() {
    if (mScope) {
      mScope->markRefsForGC();
    }
  }

  string Function::dump() {
    std::ostringstream buf;
    buf << "Function(\"" << name() << "\")";
    return buf.str();
  }

  #pragma mark Engine

  void Engine::registerForGC(GCThing *obj) {
    // Because we don't yet control the allocation, we don't know how to walk
    // the heap looking for all objects. Instead, we need to keep a separate
    // set of all objects in order to do the final sweep.
    mObjects.insert(obj);
  }

  Object *Engine::newObject(Object *prototype) {
    auto obj = new Object(prototype);
    registerForGC(obj);
    return obj;
  }

  String *Engine::newString(const string &aStr) {
    auto str = new String(aStr);
    registerForGC(str);
    return str;
  }

  Symbol *Engine::newSymbol(const string &aName) {
    auto sym = new Symbol(aName);
    registerForGC(sym);
    return sym;
  }

  Function *Engine::newFunction(
    FunctionBody aBody,
    std::string aName,
    size_t aArity,
    std::vector<Val *>aCaptures)
  {
    auto func = new Function(mScope, aBody, aName, aArity, aCaptures);
    registerForGC(func);
    return func;
  }

  Function *Engine::newFunction(
    FunctionBody aBody,
    std::string aName,
    size_t aArity)
  {
    return newFunction(aBody, aName, aArity, {});
  }

  Function *Engine::newFunction(
    FunctionBody aBody,
    std::string aName)
  {
    return newFunction(aBody, aName, 0);
  }

  Scope *Engine::newScope(size_t aLocalCount) {
    auto scope = new Scope(mScope, aLocalCount);
    registerForGC(scope);
    return scope;
  }

  Scope *Engine::pushScope(size_t aLocalCount) {
    auto scope = newScope(aLocalCount);
    mScope = scope;
    return scope;
  }

  void Engine::popScope() {
    if (mScope) {
      mScope = mScope->parent();
    } else {
      // this should not happen
      std::abort();
    }
  }

  Frame *Engine::newFrame(
    Function *aFunc,
    Val aThis,
    std::vector<Val> aArgs)
  {
    auto frame = new Frame(mFrame, aFunc, aThis, aArgs);
    registerForGC(frame);
    return frame;
  }

  Frame *Engine::pushFrame(
    Function *aFunc,
    Val aThis,
    std::vector<Val> aArgs)
  {
    auto frame = newFrame(aFunc, aThis, aArgs);
    mFrame = frame;
    return frame;
  }

  void Engine::popFrame() {
    if (mFrame) {
      mFrame = mFrame->parent();
    } else {
      // should not happen!
      std::abort();
    }
  }

  Val Engine::call(Val aFunc, Val aThis, std::vector<Val> aArgs) {
    if (aFunc.isFunction()) {
      auto func = aFunc.asFunction();
      auto frame = pushFrame(func, aThis, aArgs);
      auto retval = func->body()(this, func, frame);
      popFrame();
      return retval;
    } else {
      // todo do something
      std::abort();
    }
  }

  void Engine::gc() {
    // 1) Mark!
    mRoot->markForGC();

    if (mScope) {
      mScope->markForGC();
    }

    if (mFrame) {
      mFrame->markForGC();
    }

    // 2) Sweep!
    // Todo: don't require allocating memory to free memory!
    std::vector<GCThing *> deadObjects;
    for (auto obj : mObjects) {
      if (!obj->isMarkedForGC()) {
        deadObjects.push_back(obj);
      }
    }

    for (auto obj : deadObjects) {
      if (obj->isMarkedForGC()) {
        // Keep the object, but reset the marker value for next time.
        obj->clearForGC();
      } else {
        // No findable references to this object. Destroy it!
        mObjects.erase(obj);
        delete obj;
      }
    }
  }

  string Engine::dump() {
    std::ostringstream buf;
    buf << "Engine([";

    bool first = true;
    for (auto obj : mObjects) {
      if (first) {
        first = false;
      } else {
        buf << ",";
      }
      buf << obj->dump();
    }
    buf << "])";
    return buf.str();
  }
}
