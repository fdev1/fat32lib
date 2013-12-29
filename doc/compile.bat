@"c:\program files (x86)\doxygen\doxygen.exe" fat32lib.doxy.txt
@doxygen_preprocessor.exe "Fat32Lib" "Introduction" "pages\main_template.html" "pages\temp\main.html" "html\index.html"
@doxygen_preprocessor.exe "Fat32Lib" "Compile Instructions" "pages\compile_template.html" "pages\temp\compile.html" "html\index.html"
@doxygen_preprocessor.exe "Fat32Lib" "Features" "pages\features_template.html" "pages\temp\features.html" "html\index.html"
@doxygen_preprocessor.exe "Fat32Lib" "Examples" "pages\examples_template.html" "pages\temp\examples.html" "html\index.html"
@doxygen_preprocessor.exe "Fat32Lib" "Licensing" "pages\licensing_template.html" "pages\temp\licensing.html" "html\index.html"
@doxygen_preprocessor.exe "Fat32Lib" "References" "pages\references_template.html" "pages\temp\references.html" "html\index.html"
rem @pause
@"c:\program files (x86)\doxygen\doxygen.exe" fat32lib.doxy.txt
rem @pause