/*
 Copyright 2018 - Ivan Landry

 This file is part of WebRadio.

WebRadio is free software: you can redistribute it and/or modify
it under the terms of the GNU Affero General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

WebRadio is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU Affero General Public License for more details.

You should have received a copy of the GNU Affero General Public License
along with WebRadio.  If not, see <https://www.gnu.org/licenses/>.
*/

#include "JavascriptEngine.hpp"
#include "Utils.hpp"

#include <boost/range.hpp>

#include <fstream>
#include <iostream>
#include <cstdlib>
#include <vector>
#include <regex>


namespace JSEngine
{




class Expression;
class PtrExpression;
typedef std::map<std::string, Expression*> Variables;
typedef std::vector<PtrExpression > Expressions;
typedef std::vector<std::unique_ptr<Expression> > UPtrExpressions;
typedef std::string::const_iterator cstrIt;

class Expression
{
    public:
        virtual std::string interpret() = 0;
        virtual ~Expression(){}
};


class ExpressionPool
{
    std::vector<Expression*> _exprs;

    public:
    ExpressionPool()
        : _exprs()
    {}

    ExpressionPool(const ExpressionPool &)=delete;
    ExpressionPool(ExpressionPool &&)=default;
    ExpressionPool& operator=(const ExpressionPool &)=delete;
    ExpressionPool& operator=(ExpressionPool &&)=default;

    template <typename T, typename... Args>
    std::size_t makeExpression(Args... args)
    {
        _exprs.push_back(new T(std::forward(args...)));
        return _exprs.size() - 1;
    }

    Expression *& operator[](std::size_t id)
    {
        return _exprs[id];
    }

    std::size_t push(Expression * expr)
    {
        _exprs.push_back(expr);
        return _exprs.size() - 1;
    }

    ~ExpressionPool()
    {
        std::for_each(_exprs.begin(), _exprs.end(), std::default_delete<Expression>());
    }

};

static ExpressionPool& getExpressionPool()
{
    static ExpressionPool pool;
    return pool;
}


class PtrExpression 
{
    std::size_t _id;

    public:

    PtrExpression(Expression * expr)
        : _id(getExpressionPool().push(expr))
    {}    

    Expression * operator->()
    {
        return getExpressionPool()[_id];
    }

    void reset(Expression * expr)
    {
        Expression *& exprReset = getExpressionPool()[_id];
        delete exprReset;
        exprReset = expr;
    }

};

class Str : public Expression
{
    std::string _str;
    public:
        Str(std::string str) : _str(std::move(str)){}
        std::string interpret() override { return _str;}

        std::string & getString() { return _str; }
};

class Split : public Expression
{
    std::string _arg;
    public:
        Split(std::string str) : _arg(std::move(str)){}
        std::string interpret() override { return _arg; } // string is already a char array ...
};

class Join : public Expression
{
    std::string _arg;
    public:
        Join(std::string str) : _arg(std::move(str)){}
        std::string interpret() override { return _arg; } // string is already a char array ...
};



class Reverse : public Expression
{
    PtrExpression _arg;
    public:
        Reverse(PtrExpression expr) : _arg(std::move(expr)){}
        std::string interpret() override 
        { 
            const std::string & argStr = _arg->interpret();
            LOG << " reverse string : " << argStr;
            std::string ret(argStr.crbegin(), argStr.crend());
            LOG << " reversed string : " << ret;
            return ret;
        }
};


class Splice : public Expression
{
    // so this splice() method is supposed to remove and/or add elements to an array...
    // javascript is such a messy language :)
    PtrExpression _arg;
    std::size_t _start;
    std::size_t _deleteCnt;
    UPtrExpressions _newItems;

    public:
    Splice (PtrExpression expr, std::size_t start, std::size_t deleteCnt, UPtrExpressions newItems) 
        : _arg(std::move(expr))
        , _start(start)
        , _deleteCnt(deleteCnt)
        , _newItems(std::move(newItems))
    {}

    std::string interpret() override
    {
        std::string str(_arg->interpret());
        LOG << "splice string " << str << " start : " << _start 
            << " delete cnt " << _deleteCnt << 
            " new Items size " << _newItems.size();
        _start = std::min(_start, str.size());
        if (_start < 0)
        {
            _start = std::max(static_cast<std::size_t>(0), str.size() - _start + 1);
        }
        str.erase(_start, _deleteCnt);
        for (const auto & newItem : _newItems)
        {
            const std::string & itemVal = newItem->interpret();
            if (itemVal.size() > 1)
            {
                LOG << "Splice has an argument longer than 1 char : " << itemVal;
            }
            str.insert(_start, 1, newItem->interpret().front());
        }
        LOG << "spliced string " << str;
        return str;
    }
};

class Indexed : public Expression
{
    PtrExpression _expr;
    PtrExpression _idx;

    public:
    Indexed(PtrExpression expr, PtrExpression idx)
        : _expr(std::move(expr))
        , _idx(std::move(idx))
    {}

    std::string interpret() override
    {
        const std::string & str = _expr->interpret();
        const std::string & idxStr = _idx->interpret();
        LOG << "index " << idxStr << " on string " << str;
        // idx should be a size_t
        char * strEnd;
        std::size_t idx = std::strtoull(idxStr.c_str(), &strEnd, 10);
        LOG << "indexed string " << str[idx];
        return std::string(1, str[idx]);
    }
};

class Modulo : public Expression
{
    PtrExpression _lhs;
    PtrExpression _rhs;

    public:
    Modulo(PtrExpression lhs, PtrExpression rhs)
        : _lhs(std::move(lhs))
        , _rhs(std::move(rhs))
    {}

    std::string interpret() override
    {
        const std::string & lhsStr = _lhs->interpret();
        const std::string & rhsStr = _rhs->interpret();
        LOG << "modulo on string " << lhsStr << " and " << rhsStr;
        char * strEnd1;
        const std::size_t lhsInt = std::strtoull(lhsStr.c_str(), &strEnd1, 10);
        char * strEnd2;
        const std::size_t rhsInt = std::strtoull(rhsStr.c_str(), &strEnd2, 10);
        LOG << "moduloed string " << lhsInt % rhsInt;
        return std::to_string(lhsInt % rhsInt);
    }

};

class Length : public Expression
{
    PtrExpression _expr;
    public:
    Length(PtrExpression expr)
        : _expr(std::move(expr)) 
    {}

    std::string interpret() override
    {
        const std::string & str = _expr->interpret();
        LOG << "length string " << str;
        return std::to_string(str.size() /*_expr->interpret().size()*/);
    }
};

class Assign : public Expression
{
    // atm only assign by values are used
    // therefore we have to evaluate 
    // expr value and store it 
    PtrExpression _from;
    PtrExpression _to;

    public:
    Assign(PtrExpression from, PtrExpression to)
        : _from(std::move(from))
        , _to(std::move(to))
    {}

    std::string interpret() override
    {
        const std::string & fromStr = _from->interpret();
        LOG << "assign string " << fromStr;
        _to.reset(new Str(fromStr));
        return std::string();
    }

};


class AssignIndexed : public Expression
{
    PtrExpression _from;
    PtrExpression _to;
    PtrExpression _idx;

    public:
    AssignIndexed(PtrExpression from, 
            PtrExpression to, 
            PtrExpression idx)
        : _from(std::move(from)) 
        , _to(std::move(to))
        , _idx(std::move(idx))
    {}

    std::string interpret() override
    {
        // thats dangerous, find a better way ... 
        Str * toExpr = static_cast<Str*>(_to.operator->());
        // not using interpret cause we need the reference
        std::string & toStr = toExpr->getString();
        const std::string & idxStr = _idx->interpret();
        const std::string & fromStr = _from->interpret();
        LOG << "assign " << fromStr << " to " << 
            toStr << " at index " << idxStr;
        char * endChar;
        const std::size_t idx = std::strtoull(idxStr.c_str(), &endChar, 10);
        toStr[idx] = fromStr[0];
    }

};

class Nothing : public Expression
{
    public:
    // void : empty expression
    // does literally nothing
    std::string interpret() override
    {
        return std::string();
    }

    static PtrExpression getNothing()
    {
        // no need to allocate more than 1 instance, all do the same thing
        static PtrExpression nothingExpr(new Nothing());
        return nothingExpr;
    }
};




namespace Rgx
{
    // captures fnParams -> ("") or () or (a,b,...)
    static const std::string _fnParams = "\\((\"{2}|[[:alnum:]]*[,[:alnum:]]*|)\\)";
    // captures 1: object name 2: method name -> object.method
    static const std::string _methodCall = "\\s*([[:alnum:]]+)\\.([[:alnum:]]+)\\s*";
    // captures var name assigned -> a = 
    static const std::string _assign = "\\s*([[:alnum:]]+)\\s*=\\s*";
    // captures any assignement -> ... = ...
    static const std::string _assignAny = "(.*)=(.*)";
    // captures var indexed -> a[idx]
    static const std::string _indexed = "([[:alnum:]]+)\\[(.*)\\]";
    //captures assign to indexed 
    static const std::string _assignIndexed = _indexed + "=(.*)";
    // captures var name defined -> var a = 
    static const std::string _define = "\\s*var\\s+([[:alnum:]]+)\\s*=\\s*";
    // captures function arguments and body -> : function(arguments) { body }
    static const std::string _defineFunctionNoName = 
        "\\s*[=:]\\s*function\\((\\s*[[:alnum:]]*[\\s,[:alnum:]]*)\\)\\{(.*)\\}";
    // captures function name, arguments and body -> fnName : function(arguments) { body }
    static const std::string _defineFunction = 
        "\\s*([[:alnum:]]+)\\s*" + _defineFunctionNoName;
    // captures modulo call -> a%b
    static const std::string _modulo = "^(.+)%(.+)$";
    // capture signature decipher function name
    static const std::string _sigFn = "\"signature\"\\s*,\\s*([[:alnum:]]+)\\(";

    static const std::regex AssignMethod(_assign + _methodCall + _fnParams);
    static const std::regex AssignAny(_assignAny);
    static const std::regex MethodCall(_methodCall + _fnParams); 
    static const std::regex InstanceProperty(_methodCall); 
    static const std::regex DefineFunction(_defineFunction);
    static const std::regex Indexed(_indexed);
    static const std::regex DefineFromIndexed(_define + _indexed);
    static const std::regex AssignIndexed(_assignIndexed);
    static const std::regex Modulo(_modulo);
    static const std::regex SignatureFunction(_sigFn);
//    static const std::regex rgx(fnName + "\\s*[=:]\\s*function\\((\\s*[[:alnum:]]*[\\s,[:alnum:]]*)\\)\\{(.*)\\}");

    // captures var definition -> var a = { definition };
    std::regex defineVar(const std::string & varName)
    {
        return std::regex("\\s*var\\s+"+varName+"\\s*=\\s*\\{([\\S\\s]*?)\\};");
    }

    // captures function arguments and body -> fnName : function(arguments) { body }
    std::regex defineFunction(const std::string & fnName)
    {
        return std::regex(fnName + _defineFunctionNoName);
    }
}

void split(const std::string & str, char delim, const std::function<void(cstrIt, cstrIt)> & op)
{
    if (str.empty())
    {
        return;
    }

    auto itBeg = str.cbegin();
    auto itEnd = str.cend();

    for (;;)
    {
        itEnd = std::find(itBeg, str.cend(), delim);
        op(itBeg, itEnd);
        if (itEnd != str.cend())
        {
            itBeg = ++itEnd;
        }
        else
        {
            return;
        }
    } 
}

void split(const std::string & str, char delim, const std::function<void(std::string &&)> & op)
{
    split(str, delim, [&op](cstrIt itBeg, cstrIt itEnd){ op(std::string(itBeg, itEnd)); });
}

bool isInteger(const std::string & str) noexcept
{
    return str.find_first_not_of("0123456789") == std::string::npos;
}

class Function : public Expression
{
    const std::string _name;
    const std::string _code;
    const std::string _vars;
    std::map<std::string, PtrExpression > _varMap;
    std::map<std::string, PtrExpression > _fnMap;
    Expressions _stack;

    public:
    Function() = default;
    Function(const Function&) = delete;
    Function& operator=(const Function&) = delete;
    ~Function() = default;
    Function(Function && other) = default;
    Function& operator=(Function &&) = default;

    explicit Function(std::string name, std::string vars, std::string code)
        : _name(std::move(name))
        , _vars(std::move(vars))
        , _code(std::move(code))
        , _varMap()
    {
        LOG << "function created, name : " 
            << _name << " code : " << _code 
            << " vars : " << _vars;
    }

    explicit Function(const std::smatch & sm) 
        : Function(sm[1], sm[2], sm[3])
    {}


    void setArguments(const Expressions & args)
    {
        _varMap.clear();
        split(_vars, ',', [this, &args](std::string && varName)
                {
                this->_varMap.insert(
                        std::make_pair(varName, args[this->_varMap.size()]));
                });
    }


    std::string getVar(const std::string & varName)  
    {
        auto it = _varMap.find(varName);
        return it->second->interpret();        
    }


    PtrExpression parseMethodCall(
            const PtrExpression & instance, 
            const std::string & methodName, 
            const std::string & methodArgs)
    {
        LOG << "call method " << methodName << " with args " << methodArgs;
        if (methodName == "split" || methodName == "join")
        {
            // nothing to do atm, string is already a char array in C++
            return instance;
        }
        else if (methodName == "reverse")
        {
            return PtrExpression(new Assign(
                    PtrExpression(new Reverse(instance)),
                    instance));
        }
        else if (methodName == "length")
        {
            return PtrExpression(new Length(instance));
        }
        else if (methodName == "splice")
        {
            int idx = 0;
            std::size_t spliceStart = 0;
            std::size_t spliceRemoveCnt = 0;
            UPtrExpressions spliceArgs;
            split(methodArgs, ',', 
                    [this, &idx, &spliceStart, &spliceRemoveCnt, &spliceArgs]
                    (std::string && argStr)
                    {
                    switch (idx)
                    {
                    case 0:
                    {
                    const std::string & startStr = this->parseCode(argStr, "")->interpret();
                    char * strEnd;
                    spliceStart = std::strtoull(startStr.c_str(), &strEnd, 10);
                    }
                    break;
                    
                    case 1:
                    {
                    const std::string & removeCntStr = this->parseCode(argStr, "")->interpret();
                    char * strEnd2;
                    spliceRemoveCnt = std::strtoull(removeCntStr.c_str(), &strEnd2, 10);
                    }
                    break;
                    
                    default:
                    spliceArgs.push_back(std::unique_ptr<Expression>(new Str(argStr)));
                    break;
                    }
                    ++idx;
                    });
            return PtrExpression(new Assign(
                    new Splice(instance, spliceStart, spliceRemoveCnt, std::move(spliceArgs)),
                    instance
                    ));
        }
        return Nothing::getNothing();
    }


    PtrExpression parseCode(const std::string & toParse, const std::string & jsCode) 
    {
        LOG << "search string " << toParse;

        std::smatch matches;
        auto itVarMap = _varMap.find(toParse);
        if (itVarMap != _varMap.cend())
        {
            LOG << "found var " << toParse;
            return itVarMap->second;
        }
        else if (isInteger(toParse))
        {
            LOG << "found integer " << toParse;
            return PtrExpression(new Str(toParse));
        }
        if (std::regex_search(toParse, matches, Rgx::AssignMethod))
        {
            const std::string & varStr = matches[1].str();
            const std::string & instanceStr = matches[2].str();
            const std::string & methodStr = matches[3].str();
            const std::string & methodArgs = matches[4].str();
            LOG << "found assign var " << varStr << " with method " <<
                methodStr << " from " << instanceStr << " with args " <<
                methodArgs;

            auto itVar = _varMap.find(varStr); 
            auto itInstance = _varMap.find(instanceStr);
            if (itVar == _varMap.cend())
            {
                LOG << "trying to assign undefined variable " << varStr;
            }
            else if (itInstance == _varMap.cend())
            {
                LOG << "trying to call " << methodStr << 
                    " from undefined instance " << instanceStr;
            }
            else
            {
                return PtrExpression(new Assign(
                            /*from*/parseMethodCall(itInstance->second, methodStr, methodArgs),
                            /*to*/ itVar->second));
            }
        }
        else if (std::regex_search(toParse, matches, Rgx::AssignIndexed))
        {
            const std::string & to = matches[1].str();
            const std::string & index = matches[2].str();
            const std::string & from = matches[3].str();
            LOG << "found assign " << from << " to " << to << " at index " << index; 
            return PtrExpression(new AssignIndexed(
                        parseCode(from, jsCode),
                        parseCode(to, jsCode),
                        parseCode(index, jsCode)));

        }
        else if (std::regex_search(toParse, matches, Rgx::AssignAny))
        {
            const std::string & to = matches[1].str();
            const std::string & from = matches[2].str();
            LOG << "found assign from " << from << " to " << to;

            return PtrExpression(
                    new Assign(parseCode(from, jsCode), parseCode(to, jsCode)));
        }
        else if (std::regex_search(toParse, matches, Rgx::Indexed))
        {
            const std::string & varStr = matches[1].str();
            const std::string & idxStr = matches[2].str();
            LOG << "found var " << varStr << " indexed at " << idxStr;

            return PtrExpression(new Indexed(parseCode(varStr, jsCode), parseCode(idxStr, jsCode)));
        }
        else  if (std::regex_search(toParse, matches, Rgx::MethodCall))
        {
            const std::string & varName = matches[1].str();
            const std::string & methodName = matches[2].str();
            const std::string & methodArgs = matches[3].str();

            LOG << "found method " << methodName << " from " << varName;
            auto itVar = _varMap.find(varName);
            if (itVar == _varMap.cend())
            {
                // not a local variable, this is function containing variable 
                const std::string & methodName = matches[2].str();
                const std::string & fnKey = varName+"."+methodName;
                auto itFn = _fnMap.find(fnKey);
                if (itFn == _fnMap.cend())
                {
                    // must find var def
                    std::smatch matchesVarDef;
                    LOG << " will try to find " << varName << "definition";
                    std::regex rgxVarDef(Rgx::defineVar(varName));
                    if (std::regex_search(jsCode, matchesVarDef, Rgx::defineVar(varName)))
                    {
                        LOG << "found " << matches[1] << " definition " << matchesVarDef[1];
                        // find the functions ...
                        std::smatch matchesInnerFn;
                        std::string s(matchesVarDef[1]); 
                        LOG << "rgxDefineFn " << Rgx::_defineFunction;
                        auto rgxItBeg = std::sregex_iterator(s.cbegin(), s.cend(), Rgx::DefineFunction);
                        auto rgxItEnd = std::sregex_iterator();
                        for (auto it=rgxItBeg; it!=rgxItEnd; ++it)
                        {
                           Function * fn = new Function(*it);
                           const std::string & newFnKey = varName + "." + fn->_name;
                            auto itFnInserted = _fnMap.insert(
                                    std::make_pair(newFnKey, PtrExpression(fn)));
                            if (newFnKey == fnKey)
                            {
                                itFn = itFnInserted.first;
                            }
                        }
                    }
                    else
                    {
                        LOG << "could not find var definition : " << varName;
                        return Nothing::getNothing();
                    }
                }
                if (itFn == _fnMap.cend())
                {
                    LOG << "could not find fcnt " << methodName << 
                        "definition in " << varName;
                    return Nothing::getNothing();
                }

                Expressions argValues;
                split(methodArgs, ',', [this, &argValues](std::string && s)
                        {
                        auto it = this->_varMap.find(s);
                        if (it != this->_varMap.cend())
                        {
                        argValues.push_back(it->second);
                        }
                        else 
                        // a constant, always integers in our case
                        // this might change in the future ...
                        {
                        argValues.push_back(PtrExpression(new Str(s)));
                        }
                        });
                Function * fnModel = static_cast<Function*>(itFn->second.operator->());
                Function * fn = new Function(fnModel->_name, fnModel->_vars, fnModel->_code);
                fn->setArguments(argValues);
                fn->parseCode(jsCode);
                return PtrExpression(fn);
            }
            else    
            {
                // we call a method instance on a local var
                return parseMethodCall(itVar->second, methodName, methodArgs);
            }
        }
        else if (std::regex_search(toParse, matches, Rgx::DefineFromIndexed))
        {
            const std::string & newVar = matches[1].str();
            const std::string & srcVar = matches[2].str();
            const std::string & indexStr = matches[3].str();
            LOG << "Found define " << newVar << " from " << srcVar << " indexed at " << indexStr;

            PtrExpression idxExpr = parseCode(indexStr, jsCode);

            auto itSrcVar = _varMap.find(srcVar);
            if (itSrcVar == _varMap.cend())
            {
                LOG << "could not find src var " << srcVar << " to index for define ";
                return Nothing::getNothing();
            }

            auto itNewVar = _varMap.find(newVar);
            if (itNewVar != _varMap.cend())
            {
                itNewVar->second.reset(new Indexed(itSrcVar->second, idxExpr));
            }
            else
            {
                _varMap.insert(
                        std::make_pair(newVar, 
                            PtrExpression(
                                new Indexed(itSrcVar->second, idxExpr))));
            }
        }
        else if (std::regex_search(toParse, matches, Rgx::Modulo))
        {
            const std::string & lhs = matches[1].str();
            const std::string & rhs = matches[2].str();
            LOG << "found " << lhs << " modulo " << rhs;
            return PtrExpression(new Modulo(parseCode(lhs, jsCode), parseCode(rhs, jsCode)));
        }
        else if (std::regex_search(toParse, matches, Rgx::InstanceProperty))
        {
            const std::string & varStr = matches[1].str();
            const std::string & propertyStr = matches[2].str();
            LOG << "found property " << propertyStr << " on " << varStr;

            auto itVar = _varMap.find(varStr);
            if (itVar == _varMap.cend())
            {
                LOG << "could not find var " << varStr;
            }
            else
            {
                // works like a method call without arguments
                return parseMethodCall(itVar->second, propertyStr, std::string());
            }
        }

        return Nothing::getNothing();
    }


    PtrExpression parseCode(const std::string & jsCode) 
    {
        split(_code, ';', [this, &jsCode](std::string && splitted) { 
                this->_stack.push_back(this->parseCode(splitted, jsCode)); 
                });
        return _stack.back();
    }

    std::string interpret() override
    {
        for (auto it = _stack.begin(); it != _stack.end(); ++it)
        {
            if (std::next(it) == _stack.end())
            {
                return (*it)->interpret();
            }
            (*it)->interpret();
        }
    }
};

Function findFunction(const std::string & jsCode, const std::string & fnName)
{
    // fnName = function(a, b, ...) { code } or fnName : funtion(a, b, ...) { code }
    static const std::regex rgx(fnName + "\\s*[=:]\\s*function\\((\\s*[[:alnum:]]*[\\s,[:alnum:]]*)\\)\\{(.*)\\}");
    std::smatch matches;
    if (std::regex_search(jsCode, matches, rgx))
    {
        Function f(fnName, std::move(matches[1]), std::move(matches[2]));
        return f;
    }
    else
    {
        LOG << "could not find function " << fnName;
        return Function();
    }
    
}

std::string findSignatureFnName(const std::string & jsCode)
{
    std::smatch funcNameMatch;
    std::regex rgx("\"signature\"\\s*,\\s*([[:alnum:]]+)\\(");
    if (std::regex_search(jsCode, funcNameMatch, rgx))
    {
        LOG << "function name is " << funcNameMatch[1];
        return funcNameMatch[1].str();
    }
    else
    {
        LOG << "function name not found ";
    }
    return std::string();
}

std::string decipherSignature(const std::string & jsCode, const std::string & signature)
{
    const std::string & fnName = findSignatureFnName(jsCode);
    // move
    Function signatureFunction = findFunction(jsCode, fnName);
    signatureFunction.setArguments(Expressions{PtrExpression(new Str(signature))});
    signatureFunction.parseCode(jsCode);
    return signatureFunction.interpret();
}

}

/*
int main()
{
    static const std::string sig("4F54F57EA340B453B68F8A026846D0A329180B3DCCE.D9F57E82A09AC96B32A198B384748090D547223832");
    const std::string & jsCode = JSEngine::readFile("base2.js");
    const std::string & clearSig = JSEngine::decipherSignature(jsCode, sig);

    LOG << "End sig " << clearSig;

    return EXIT_SUCCESS;
}
*/




