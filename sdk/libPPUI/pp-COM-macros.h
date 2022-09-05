#pragma once

#define COM_QI_BEGIN() HRESULT STDMETHODCALLTYPE QueryInterface(REFIID iid,void ** ppvObject) override { if (ppvObject == NULL) return E_INVALIDARG;
#define COM_QI_ENTRY(IWhat) { if (iid == __uuidof(IWhat)) {IWhat * temp = this; temp->AddRef(); * ppvObject = temp; return S_OK;} }
#define COM_QI_ENTRY_(IWhat, IID) { if (iid == IID) {IWhat * temp = this; temp->AddRef(); * ppvObject = temp; return S_OK;} }
#define COM_QI_END() * ppvObject = NULL; return E_NOINTERFACE; }

#define COM_QI_CHAIN(Parent) { HRESULT status = Parent::QueryInterface(iid, ppvObject); if (SUCCEEDED(status)) return status; }

#define COM_QI_SIMPLE(IWhat) COM_QI_BEGIN() COM_QI_ENTRY(IUnknown) COM_QI_ENTRY(IWhat) COM_QI_END()


#define PP_COM_CATCH catch(exception_com const & e) {return e.get_code();} catch(std::bad_alloc) {return E_OUTOFMEMORY;} catch(pfc::exception_invalid_params) {return E_INVALIDARG;} catch(...) {return E_UNEXPECTED;}