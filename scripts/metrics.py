"""Small source inspection tool. Decision count is a lexical approximation, not a formal CFG calculation."""
from pathlib import Path
import re
ROOT=Path(__file__).resolve().parent.parent
s=(ROOT/'src/lms.cpp').read_text()
for path in ['src/lms.cpp','src/main.cpp','include/lms/lms.hpp','tests/test_core.cpp','tests/test_catch2.cpp']:
 text=(ROOT/path).read_text()
 print(f'{path}: {len(text.splitlines())} physical lines, {sum(bool(x.strip()) for x in text.splitlines())} nonblank lines')
for name in ['submit','mark','moderate','userDeadline','releaseCourse','classExtension','lecturerSubmissions','hodSubmissions']:
 search=re.search(r'LmsService::'+name+r'\s*\([^\n]*?\)(?:\s*const)?\s*\{',s)
 if not search:continue
 start=search.end();depth=1;i=start
 while i<len(s) and depth:
  if s[i]=='{':depth+=1
  elif s[i]=='}':depth-=1
  i+=1
 body=s[start:i-1]
 clean=re.sub(r'"(?:\\.|[^"\\])*"','""',body)
 control=len(re.findall(r'\b(?:if|for|while|case|catch)\s*\(',clean))
 logic=len(re.findall(r'&&|\|\|',clean))
 ternary=clean.count('?')
 print(f'LmsService::{name}: approximate lexical decision score {1+control+logic+ternary} (1 base + {control} controls + {logic} short-circuit + {ternary} ternary)')
