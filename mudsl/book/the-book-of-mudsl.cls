\NeedsTeXFormat{LaTeX2e}
\ProvidesClass{the-book-of-mudsl}[2026/11/07 MuDSL book]
\DeclareOption*{\PassOptionsToClass{\CurrentOption}{extbook}}
\ProcessOptions\relax
\LoadClass[openany]{extbook}
\RequirePackage[margin=25mm,headheight=18pt]{geometry}
\RequirePackage{fancyhdr}
\pagestyle{fancy}
\fancyhf{}
\fancyhead[LE,RO]{\thepage}
\fancyhead[LO]{\small\nouppercase{\rightmark}}
\fancyhead[RE]{\small\nouppercase{\leftmark}}
\setcounter{tocdepth}{1}
\setcounter{secnumdepth}{2}
\raggedbottom
