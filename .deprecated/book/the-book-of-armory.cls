\NeedsTeXFormat{LaTeX2e}
\ProvidesClass{the-book-of-armory}[2026/11/09 Armory book structure]
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
% Presentation belongs to support/armory-book.sty, not the structural class.
