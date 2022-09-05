#pragma once

#include <functional>
#include "string_base.h"

namespace pfc {
	namespace io {
		namespace path {
#ifdef _WINDOWS
			typedef string::comparatorCaseInsensitive comparator;
#else
            typedef string::comparatorCaseSensitive comparator; // wild assumption
#endif
            

			typedef std::function<const char* (char)> charReplace_t;

			const char * charReplaceDefault(char);
			const char * charReplaceModern(char);

			string getFileName(string path);
			string getFileNameWithoutExtension(string path);
			string getFileExtension(string path);
			string getParent(string filePath);
			string getDirectory(string filePath);//same as getParent()
			string combine(string basePath,string fileName);
			char getDefaultSeparator();
			string getSeparators();
			bool isSeparator(char c);
			string getIllegalNameChars(bool allowWC = false);
			string replaceIllegalNameChars(string fn, bool allowWC = false, charReplace_t replace = charReplaceDefault);
			string replaceIllegalPathChars(string fn, charReplace_t replace = charReplaceDefault);
			bool isInsideDirectory(pfc::string directory, pfc::string inside);
			bool isDirectoryRoot(string path);
			string validateFileName(string name, bool allowWC = false, bool preserveExt = false, charReplace_t replace = charReplaceDefault);//removes various illegal things from the name, exact effect depends on the OS, includes removal of the invalid characters

			template<typename t1, typename t2> inline bool equals(const t1 & v1, const t2 & v2) {return comparator::compare(v1,v2) == 0;}

            template<typename t1, typename t2> inline int compare( t1 const & p1, t2 const & p2 ) {return comparator::compare(p1, p2); }
		}
	}
}
