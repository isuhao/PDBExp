#include "LvStd.h"
#include "SymWrap.h"
#include <sstream>

CSym::CSym(__in IDiaSymbol* sym)
{
    m_sym = sym;
    m_sym->AddRef();
}

CSym::~CSym(void)
{
    m_sym->Release();
}

void CSym::Declare(__out wstring* str, __in PCWSTR lpName)
{
    wstring strType;
    GetType(&strType);
    str->assign(strType);
    str->append(1, L' ');
    str->append(lpName);
}

void CSym::Delete(__in CSym* sym)
{
    delete sym;
}

IDiaSymbol* CSym::Enum(
    __in IDiaSymbol* symParent,
    __in enum SymTagEnum enTag,
    __in EnumProc cbEnum,
    __in_opt PVOID param)
{
    if (NULL == symParent)
        return NULL;

    CComPtr<IDiaEnumSymbols> pEnum = NULL;
    HRESULT hr = symParent->findChildren(enTag, NULL, nsNone, &pEnum);
    if (SUCCEEDED(hr) && pEnum)
    {
        ULONG count = 1;
        IDiaSymbol* curItem = NULL;
        pEnum->Next(1, &curItem, &count);
        do
        {
            if (NULL != curItem)
            {
                if (cbEnum(curItem, param))
                    return curItem;

                curItem->Release();
            }

            pEnum->Next(1, &curItem, &count);
        } while (0 != count);
    }

    return NULL;
}
    
BOOL CSym::Format(__out wstring* str)
{
    enum SymTagEnum tag;
    CComBSTR bsName;
    m_sym->get_name(&bsName);
    m_sym->get_symTag((PDWORD)&tag);

    ATLTRACE(L"CSym::Format [%d] %s.\n", tag, bsName.m_str);
    ATLASSERT(false);
    return FALSE;
}

BOOL CSym::GetHeader(__out wstring* str)
{
    return FALSE;
}

BOOL CSym::GetType(__out wstring* str)
{
    enum SymTagEnum tag;
    CComBSTR bsName;
    m_sym->get_name(&bsName);
    m_sym->get_symTag((PDWORD)&tag);

    ATLTRACE(L"CSym::GetType [%d] %s.\n", tag, bsName.m_str);
    ATLASSERT(false);
    return FALSE;
}

CSym* CSym::NewSym(__in IDiaSymbol* sym)
{
    enum SymTagEnum tag = SymTagNull;
    sym->get_symTag((PDWORD)&tag);

    CSym* ret = NULL;
    switch (tag)
    {
    case SymTagFunction:
        ret = new CSymFunction(sym);
        break;
    case SymTagData:
        ret = new CSymData(sym);
        break;
    case SymTagUDT:
        ret = new CSymUDT(sym);
        break;
    case SymTagEnum:
        ret = new CSymEnum(sym);
        break;
    case SymTagFunctionType:
        ret = new CSymFunctionType(sym);
        break;
    case SymTagPointerType:
        ret = new CSymPointerType(sym);
        break;
    case SymTagArrayType:
        ret = new CSymArrayType(sym);
        break;
    case SymTagBaseType:
        ret = new CSymBaseType(sym);
        break;
    case SymTagTypedef:
        ret = new CSymTypedef(sym);
        break;
    case SymTagFunctionArgType:
        ret = new CSymFunctionArgType(sym);
        break;
    default:
        ret = new CSym(sym);
    }
    return ret;
}

void CSym::TypeDefine(__out wstring* str, __in PCWSTR lpType)
{
    wstring strType;
    GetType(&strType);
    str->assign(L"<font class=\"key\">typedef</font> ");
    str->append(strType);
    str->append(1, L' ');
    str->append(lpType);
    str->append(1, ';');
}

BOOL CSymFunction::Format(__out wstring* str)
{
    CComBSTR bsName;
    m_sym->get_name(&bsName);

    SymPtr type;
    m_sym->get_type(&type);

    wstring strDeclare;
    wstring strName;
    PCWSTR call = CSymFunctionType::GetCallType(type);
    if (NULL == call || L'\0' == *call) {
        strName = bsName;
    } else {
        strName = call;
        strName.append(1, L' ');
        strName.append(bsName);
    }

    CSym* sym = CSym::NewSym(type);
    sym->Declare(&strDeclare, strName.c_str());
    CSym::Delete(sym);

    // Virtual
    BOOL bVirtual = FALSE;
    m_sym->get_virtual(&bVirtual);
    if (bVirtual) {
        str->assign(L"&nbsp;&nbsp;&nbsp;&nbsp;<font class=\"key\">virtual</font> ");
    } else {
        str->assign(L"&nbsp;&nbsp;&nbsp;&nbsp;%s");
    }
    str->append(strDeclare);

    // Pure
    BOOL bPure = FALSE;
    m_sym->get_pure(&bPure);
    if (bPure)
        *str += L" = 0";
    *str += L";<br />";
    return TRUE;
}

BOOL CSymData::Format(__out wstring* str)
{
    SymPtr symType;
    HRESULT hr = m_sym->get_type(&symType);
    if (FAILED(hr))
        return FALSE;

    CComBSTR bsName;
    m_sym->get_name(&bsName);

    // declare
    wstring strDeclare;
    CSym* type = CSym::NewSym(symType);
    type->Declare(&strDeclare, (PCWSTR)bsName);
    CSym::Delete(type);

    // size & offset
    ULONGLONG size = 0;
    LONG offset = 0;
    symType->get_length(&size);
    m_sym->get_offset(&offset);

    // "    <type> <var>; // +<offset>(<size>)"
    wstringstream stm(L"&nbsp;&nbsp;&nbsp;&nbsp;");
    stm << strDeclare
        << L"<font class=\"comment\">// +0x" << hex << offset << L"(0x" << size << L")</font>"
        << L"<br />\r\n";
    *str = stm.str();
    return TRUE;
}

BOOL CSymUDT::EnumBase(IDiaSymbol* sym, PVOID param)
{
    wstring* str = (wstring*)param;

    CComBSTR bsName;
    sym->get_name(&bsName);

    CV_access_e enAccess = CV_private;
    sym->get_access((PDWORD)&enAccess);
    PCWSTR access = NULL;
    switch (enAccess)
    {
    case CV_private:
        access = L"<font class=\"key\">private</font>";
        break;
    case CV_protected:
        access = L"<font class=\"key\">protected</font>";
        break;
    case CV_public:
        access = L"<font class=\"key\">public</font>";
        break;
    }
    ATLASSERT(NULL != access);

    wstring strAccess(str->empty() ? L" : " : L", ");
    strAccess.append(access);
    strAccess.append(1, L' ');
    strAccess.append(bsName);
    *str += strAccess;
    return FALSE;
}

BOOL CSymUDT::EnumMember(IDiaSymbol* sym, PVOID param)
{
    SymAccess* p = (SymAccess*)param;

    wstring strMember;
    CSym* symMember = CSym::NewSym(sym);
    if (NULL != symMember)
    {
        symMember->Format(&strMember);
        CSym::Delete(symMember);

        CV_access_e access;
        sym->get_access((PDWORD)&access);
        switch (access)
        {
        case CV_private:
            p->strPrivate += strMember;
            break;
        case CV_protected:
            p->strProtected += strMember;
            break;
        case CV_public:
            p->strPublic += strMember;
            break;
        }
    }
    return FALSE;
}

BOOL CSymUDT::Format(__out wstring* str)
{
    UdtKind enKind = (UdtKind)-1;
    HRESULT hr = m_sym->get_udtKind((PDWORD)&enKind);
    if (FAILED(hr))
        return FALSE;

    // Header
    GetHeader(str);

    SymAccess member;
    int lenPublic;
    int lenProtected;
    int lenPrivate;

    // function
    CSym::Enum(m_sym, SymTagFunction, EnumMember, &member);
    lenPublic = member.strPublic.length();
    lenProtected = member.strProtected.length();
    lenPrivate = member.strPrivate.length();
    if (0 != lenPublic)
    {
        // struct 默认不带 public
        if (UdtClass == enKind || 0 != lenProtected || 0 != lenPrivate)
            *str += L"<font class=\"key\">public</font>:<br />\r\n";
        *str += member.strPublic;
    }
    if (0 != lenProtected)
    {
        *str += L"<font class=\"key\">protected</font>:<br />\r\n";
        *str += member.strProtected;
    }
    if (0 != lenPrivate)
    {
        *str += L"<font class=\"key\">private</font>:<br />\r\n";
        *str += member.strPrivate;
    }

    // data
    member.strPublic = L"";
    member.strProtected = L"";
    member.strPrivate = L"";
    CSym::Enum(m_sym, SymTagData, EnumMember, &member);
    lenPublic = member.strPublic.length();
    lenProtected = member.strProtected.length();
    lenPrivate = member.strPrivate.length();
    if (0 != lenPublic)
    {
        // struct 默认不带 public
        if (UdtClass == enKind || 0 != lenProtected || 0 != lenPrivate)
            *str += L"<font class=\"key\">public</font>:<br />\r\n";
        *str += member.strPublic;
    }
    if (0 != lenProtected)
    {
        *str += L"<font class=\"key\">protected</font>:<br />\r\n";
        *str += member.strProtected;
    }
    if (0 != lenPrivate)
    {
        *str += L"<font class=\"key\">private</font>:<br />\r\n";
        *str += member.strPrivate;
    }

    // tail
    *str += L"};";
    return TRUE;
}

BOOL CSymUDT::GetHeader(__out wstring* str)
{
    UdtKind enKind = (UdtKind)-1;
    HRESULT hr = m_sym->get_udtKind((PDWORD)&enKind);
    if (FAILED(hr))
        return FALSE;

    PCWSTR tmp = NULL;
    switch (enKind)
    {
    case UdtStruct:
        tmp = L"struct";
        break;
    case UdtClass:
        tmp = L"class";
        break;
    case UdtUnion:
        tmp = L"union";
        break;
    default:
        ATLASSERT(false);
    }

    CComBSTR bsName;
    ULONGLONG size;
    m_sym->get_name(&bsName);
    m_sym->get_length(&size);

    // base
    wstring strBase;
    CSym::Enum(m_sym, SymTagBaseClass, EnumBase, &strBase);

    // header
    wstringstream stm(L"<font class=\"key\">");
    stm << tmp << L"</font> " << bsName << strBase
        << L" <font class=\"comment\">// 0x" << hex << size << L"</font><br />{<br />\r\n";
    *str = stm.str();
    return TRUE;
}

BOOL CSymUDT::GetType(__out wstring* str)
{
    UdtKind enKind = (UdtKind)-1;
    HRESULT hr = m_sym->get_udtKind((PDWORD)&enKind);
    if (FAILED(hr))
        return FALSE;

    PCWSTR kind = NULL;
    switch (enKind)
    {
    case UdtStruct:
        kind = L"struct";
        break;
    case UdtClass:
        kind = L"class";
        break;
    case UdtUnion:
        kind = L"union";
        break;
    default:
        ATLASSERT(false);
    }

    DWORD id;
    CComBSTR bsName;
    m_sym->get_symIndexId(&id);
    m_sym->get_name(&bsName);

    wstringstream stm(L"<font class=\"key\">");
    stm << kind << L"</font> <a href=\"sym://" << id << L"\">" << bsName << L"</a>";
    *str = stm.str();
    return TRUE;
}

BOOL CSymEnum::Format(__out wstring* str)
{
    // header
    GetHeader(str);

    // members
    CSym::Enum(m_sym, SymTagData, OnEnum, str);

    // tail
    *str += L"};";
    return TRUE;
}

BOOL CSymEnum::GetHeader(__out wstring* str)
{
    CComBSTR bsName;
    m_sym->get_name(&bsName);
    str->assign(L"<font class=\"key\">enum</font> ");
    str->append(bsName);
    str->append(L"<br />{ <br />\r\n");
    return TRUE;
}

BOOL CSymEnum::GetType(__out wstring* str)
{
    CComBSTR bsName;
    m_sym->get_name(&bsName);

    DWORD id;
    m_sym->get_symIndexId(&id);

    wstringstream stm(L"<font class=\"key\">enum</font> <a href=\"sym://");
    stm << id << L"\">" << bsName << L"</a>";
    *str = stm.str();
    return TRUE;
}

BOOL CSymEnum::OnEnum(IDiaSymbol* sym, PVOID param)
{
    wstring* str = (wstring*)param;

    CComVariant v;
    CComBSTR bsName;
    sym->get_name(&bsName);
    sym->get_value(&v);

    wstringstream stm(L"&nbsp;&nbsp;&nbsp;&nbsp;");
    stm << bsName << L" = 0x" << hex << v.intVal << L";<br />\r\n";
    *str += stm.str();
    return FALSE;
}

void CSymFunctionType::Declare(__out wstring* str, __in PCWSTR lpName)
{
    wstring strRet;
    BOOL bCon = FALSE;
    m_sym->get_constructor(&bCon);
    if (!bCon && L'~' != *lpName)
    {
        // 非构造函数，需判断返回值
        SymPtr type;
        m_sym->get_type(&type);
        CSym* sym = CSym::NewSym(type);
        sym->GetType(&strRet);
        CSym::Delete(sym);
        strRet += L" ";
    }

    wstring strArgs;
    CSym::Enum(m_sym, SymTagFunctionArgType, EnumArg, &strArgs);
    if (0 == strArgs.length()) {
        strArgs = L"<font class=\"key\">void</font>";
    }

    str->assign(strRet);
    str->append(lpName);
    str->append(1, L'(').append(strArgs).append(1, L')');
}

PCWSTR CSymFunctionType::GetCallType(IDiaSymbol* sym)
{
    CV_call_e enCall;
    sym->get_callingConvention((PDWORD)&enCall);

    PCWSTR ret = L"";
    switch (enCall)
    {
    case CV_CALL_NEAR_STD:
        ret = L"<font class=\"key\">__stdcall</font>";
        break;
    }
    return ret;
}

BOOL CSymFunctionType::EnumArg(IDiaSymbol* sym, PVOID param)
{
    wstring* str = (wstring*)param;

    wstring strArg;
    CSym* symArg = CSym::NewSym(sym);
    symArg->Format(&strArg);
    CSym::Delete(symArg);

    if (!str->empty())
        *str += L", ";
    *str += strArg;
    return FALSE;
}

void CSymPointerType::Declare(__out wstring* str, __in PCWSTR lpName)
{
    SymPtr type;
    HRESULT hr = m_sym->get_type(&type);
    if (FAILED(hr))
        return;

    enum SymTagEnum tag;
    type->get_symTag((PDWORD)&tag);
    if (SymTagFunctionType != tag)
        return CSym::Declare(str, lpName);

    PCWSTR call = CSymFunctionType::GetCallType(type);
    wstring strName(1, L'(');
    if (L'\0' == *call) {
        strName.append(1, L'*');
    } else {
        strName.append(call);
        strName.append(L" * ");
    }
    strName.append(lpName).append(1, L')');

    CSym* sym = CSym::NewSym(type);
    sym->Declare(str, strName.c_str());
    CSym::Delete(sym);
}

BOOL CSymPointerType::GetType(__out wstring* str)
{
    SymPtr pointee;
    HRESULT hr = m_sym->get_type(&pointee);
    if (FAILED(hr))
        return FALSE;

    CSym* type = CSym::NewSym(pointee);
    type->GetType(str);
    CSym::Delete(type);

    BOOL bRef = FALSE;
    m_sym->get_reference(&bRef);
    if (bRef) {
        str->append(1, L'&');
    } else {
        str->append(1, L'*');
    }
    return TRUE;
}

void CSymPointerType::TypeDefine(__out wstring* str, __in PCWSTR lpType)
{
    SymPtr type;
    HRESULT hr = m_sym->get_type(&type);
    if (FAILED(hr))
        return;

    enum SymTagEnum tag;
    type->get_symTag((PDWORD)&tag);
    if (SymTagFunctionType != tag)
        return CSym::TypeDefine(str, lpType);

    PCWSTR call = CSymFunctionType::GetCallType(type);
    wstring strName(1, L'(');
    if (L'\0' == *call) {
        strName.append(1, L'*');
    } else {
        strName.append(call);
        strName.append(L" * ");
    }
    strName.append(lpType).append(1, ')');

    wstring strType;
    CSym* sym = CSym::NewSym(type);
    sym->Declare(&strType, strName.c_str());
    CSym::Delete(sym);

    str->assign(L"<font class=\"key\">typedef</font> ");
    str->append(strType).append(1, ';');
}

void CSymArrayType::Declare(__out wstring* str, __in PCWSTR lpName)
{
    SymPtr type;
    m_sym->get_type(&type);

    ULONGLONG arrsize;
    ULONGLONG elemsize;
    m_sym->get_length(&arrsize);
    type->get_length(&elemsize);

    wstringstream stm(lpName);
    stm << L"[0x" << hex << arrsize / elemsize << ']';

    CSym* sym = CSym::NewSym(type);
    sym->Declare(str, stm.str().c_str());
    CSym::Delete(sym);
}

BOOL CSymBaseType::GetType(__out wstring* str)
{
    BasicType enType = btNoType;
    HRESULT hr = m_sym->get_baseType((LPDWORD)&enType);
    if (FAILED(hr))
        return FALSE;

    ULONGLONG len = 0;
    hr = m_sym->get_length(&len);
    PCWSTR type = L"BaseType";
    switch (enType)
    {
    case btVoid:
        type = L"<font class=\"key\">void</font>";
        break;
    case btChar:
        type = L"<font class=\"key\">char</font>";
        break;
    case btWChar:
        type = L"WCHAR";
        break;
    case btInt:
        {
            switch (len)
            {
            case 1:
                type = L"<font class=\"key\">char</font>";
                break;
            case 2:
                type = L"<font class=\"key\">short</font>";
                break;
            case 4:
                type = L"<font class=\"key\">int</font>";
                break;
            case 8:
                type = L"<font class=\"key\">__int64</font>";
                break;
            }
        }
        break;
    case btUInt:
        {
            switch (len)
            {
            case 1:
                type = L"BYTE";
                break;
            case 2:
                type = L"WORD";
                break;
            case 4:
                type = L"DWORD";
                break;
            case 8:
                type = L"ULONGLONG";
                break;
            }
        }
        break;
    case btFloat:
        {
            switch (len)
            {
            case 4:
                type = L"<font class=\"key\">float</font>";
                break;
            case 8:
                type = L"<font class=\"key\">double</font>";
                break;
            }
        }
        break;
    case btBCD:
        type = L"BCD";
        break;
    case btBool: 
        type = L"<font class=\"key\">bool</font>";
        break;
    case btLong: 
        type = L"<font class=\"key\">long</font>";
        break;
    case btULong: 
        type = L"ULONG";
        break;
    case btCurrency: 
        type = L"CURRENCY";
        break;
    case btDate: 
        type = L"DATE";
        break;
    case btVariant: 
        type = L"VARIANT";
        break;
    case btComplex:
        type = L"COMPLEX";
        break;
    case btBit:
        type = L"BIT";
        break;
    case btBSTR:
        type = L"BSTR";
        break;
    case btHresult:
        type = L"HRESULT";
        break;
    }

    *str = type;
    return TRUE;
}

BOOL CSymTypedef::Format(__out wstring* str)
{
    CComBSTR bsName;
    m_sym->get_name(&bsName);

    SymPtr type;
    m_sym->get_type(&type);

    wstring strType;
    CSym* sym = CSym::NewSym(type);
    sym->TypeDefine(str, bsName);
    CSym::Delete(sym);
    return TRUE;
}

BOOL CSymFunctionArgType::Format(__out wstring* str)
{
    SymPtr type;
    m_sym->get_type(&type);

    CSym* sym = CSym::NewSym(type);
    sym->GetType(str);
    CSym::Delete(sym);
    return TRUE;
}
