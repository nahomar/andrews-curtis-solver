// Exact native counterparts of the existing Python word kernels.
#define PY_SSIZE_T_CLEAN
#include <Python.h>
#include <algorithm>
#include <array>
#include <cstdint>
#include <vector>
using Word=std::vector<int>;
static bool read_word(PyObject* obj,Word& out){
 PyObject* seq=PySequence_Fast(obj,"word must be a sequence");if(!seq)return false;
 Py_ssize_t n=PySequence_Fast_GET_SIZE(seq);if(n>4096){Py_DECREF(seq);PyErr_SetString(PyExc_ValueError,"word length guard");return false;}
 out.reserve(n);
 for(Py_ssize_t i=0;i<n;i++){long v=PyLong_AsLong(PySequence_Fast_GET_ITEM(seq,i));if(PyErr_Occurred()){Py_DECREF(seq);return false;}if(v < -8 || v > 8 || v==0){Py_DECREF(seq);PyErr_SetString(PyExc_ValueError,"generator out of range");return false;}out.push_back(int(v));}
 Py_DECREF(seq);return true;
}
static PyObject* distance(PyObject*,PyObject* args){
 PyObject *aa,*bb;if(!PyArg_ParseTuple(args,"OO",&aa,&bb))return nullptr;Word a,b;if(!read_word(aa,a)||!read_word(bb,b))return nullptr;
 if(a.empty())return PyLong_FromSize_t(b.size());
 if(a.size()<=63){
  uint64_t mask=(uint64_t(1)<<a.size())-1,pv=mask,mv=0,high=uint64_t(1)<<(a.size()-1);int score=int(a.size());std::array<uint64_t,17> eq{};
  for(size_t i=0;i<a.size();i++)eq[a[i]+8]|=uint64_t(1)<<i;
  for(int c:b){uint64_t e=eq[c+8],xv=e|mv,xh=(((e&pv)+pv)^pv)|e,ph=mv|~(xh|pv),mh=pv&xh;score+=int(bool(ph&high))-int(bool(mh&high));ph=(ph<<1)|1;mh<<=1;pv=(mh|~(xv|ph))&mask;mv=ph&xv;}
  return PyLong_FromLong(score);
 }
 std::vector<int> row(b.size()+1),next(b.size()+1);for(size_t j=0;j<=b.size();j++)row[j]=int(j);
 for(size_t i=0;i<a.size();i++){next[0]=int(i)+1;for(size_t j=1;j<=b.size();j++)next[j]=std::min({next[j-1]+1,row[j]+1,row[j-1]+int(a[i]!=b[j-1])});row.swap(next);}
 return PyLong_FromLong(row.back());
}
static PyObject* word_class(PyObject*,PyObject* obj){
 Word w;if(!read_word(obj,w))return nullptr;size_t first=0,last=w.size();while(last-first>1&&w[first]==-w[last-1]){first++;last--;}
 Word a(w.begin()+first,w.begin()+last),inverse;for(auto it=a.rbegin();it!=a.rend();++it)inverse.push_back(-*it);Word best=a;
 for(const Word* v:{&a,&inverse})for(size_t k=0;k<v->size();k++){Word candidate;candidate.insert(candidate.end(),v->begin()+k,v->end());candidate.insert(candidate.end(),v->begin(),v->begin()+k);if(candidate<best)best=std::move(candidate);}
 PyObject* result=PyTuple_New(best.size());if(!result)return nullptr;for(size_t i=0;i<best.size();i++)PyTuple_SET_ITEM(result,i,PyLong_FromLong(best[i]));return result;
}
static PyMethodDef methods[]={{"distance",distance,METH_VARARGS,"Exact Levenshtein distance."},{"word_class",word_class,METH_O,"Exact inverse/rotation cyclic class."},{nullptr,nullptr,0,nullptr}};
static struct PyModuleDef module={PyModuleDef_HEAD_INIT,"_acc_word_kernels",nullptr,-1,methods};
PyMODINIT_FUNC PyInit__acc_word_kernels(){return PyModule_Create(&module);}
