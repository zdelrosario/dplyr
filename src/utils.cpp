#include <dplyr/main.h>

#include <tools/utils.h>
#include <dplyr/white_list.h>

using namespace Rcpp;
using namespace dplyr;

// [[Rcpp::export]]
void assert_all_white_list(const DataFrame& data) {
  // checking variables are on the white list
  int nc = data.size();
  for (int i=0; i<nc; i++) {
    if (!white_list(data[i])) {
      CharacterVector names = data.names();
      String name_i = names[i];
      SEXP v = data[i];

      SEXP klass = Rf_getAttrib(v, R_ClassSymbol);
      if (!Rf_isNull(klass)) {
        stop("column '%s' has unsupported class : %s",
             name_i.get_cstring() , get_single_class(v));
      }
      else {
        stop("column '%s' has unsupported type : %s",
             name_i.get_cstring() , Rf_type2char(TYPEOF(v)));
      }

    }
  }
}

SEXP shared_SEXP(SEXP x) {
  SET_NAMED(x, 2);
  return x;
}

// [[Rcpp::export]]
SEXP shallow_copy(const List& data) {
  int n = data.size();
  List out(n);
  for (int i=0; i<n; i++) {
    out[i] = shared_SEXP(data[i]);
  }
  copy_attributes(out, data);
  return out;
}

SEXP pairlist_shallow_copy(SEXP p) {
  Shield<SEXP> attr(Rf_cons(CAR(p), R_NilValue));
  SEXP q = attr;
  SET_TAG(q, TAG(p));
  p = CDR(p);
  while (!Rf_isNull(p)) {
    Shield<SEXP> s(Rf_cons(CAR(p), R_NilValue));
    SETCDR(q, s);
    q = CDR(q);
    SET_TAG(q, TAG(p));
    p = CDR(p);
  }
  return attr;
}

bool copy_only_attributes(SEXP out, SEXP data) {
  List att = ATTRIB(data);
  const bool has_attributes = (att.length() > 0);
  if (has_attributes) {
    LOG_VERBOSE << "copying attributes: " << CharacterVector(att.names());

    SET_ATTRIB(out, pairlist_shallow_copy(ATTRIB(data)));
  }
  return has_attributes;
}

bool copy_attributes(SEXP out, SEXP data) {
  SET_OBJECT(out, OBJECT(data));
  if (IS_S4_OBJECT(data)) SET_S4_OBJECT(out);
  return copy_only_attributes(out, data);
}

// same as copy_attributes but without names
bool copy_most_attributes(SEXP out, SEXP data) {
  if (!copy_attributes(out, data))
    return false;

  LOG_VERBOSE << "dropping name attribute";
  Rf_setAttrib(out, R_NamesSymbol, R_NilValue);
  return true;
}

bool copy_column_attributes(SEXP out, SEXP data) {
  if (!copy_most_attributes(out, data))
    return false;

  LOG_VERBOSE << "dropping dim and dimnames attributes";
  Rf_setAttrib(out, R_DimSymbol, R_NilValue);
  Rf_setAttrib(out, R_DimNamesSymbol, R_NilValue);
  return true;
}

std::string get_single_class(SEXP x) {
  SEXP klass = Rf_getAttrib(x, R_ClassSymbol);
  if (!Rf_isNull(klass)) {
    CharacterVector classes(klass);
    return collapse<STRSXP>(classes);
  }

  if (Rf_isMatrix(x)) {
    return "matrix";
  }

  switch (TYPEOF(x)) {
  case INTSXP:
    return "integer";
  case REALSXP :
    return "numeric";
  case LGLSXP:
    return "logical";
  case STRSXP:
    return "character";

  case VECSXP:
    return "list";
  default:
    break;
  }

  // just call R to deal with other cases
  // we could call R_data_class directly but we might get a "this is not part of the api"
  klass = Rf_eval(Rf_lang2(Rf_install("class"), x), R_GlobalEnv);
  return CHAR(STRING_ELT(klass,0));
}
