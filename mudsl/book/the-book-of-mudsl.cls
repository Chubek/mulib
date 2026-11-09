\NeedsTeXFormat{LaTeX2e}
\ProvidesClass{the-book-of-mudsl}[2026/11/09 MuDSL book structure]
\newif\ifMu@openright
\DeclareOption{openright}{\Mu@openrighttrue}
\DeclareOption{openany}{\Mu@openrightfalse}
\DeclareOption*{\PassOptionsToClass{\CurrentOption}{extbook}}
\ProcessOptions*\relax
\ifMu@openright
  \LoadClass[openright]{extbook}
\else
  \LoadClass[openany]{extbook}
\fi
% Presentation belongs to support/mudsl-book.sty, not the structural class.
