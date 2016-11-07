/* Copyright 2016 ConvertMdToPdf authors
*
* This file is part of ConvertMdToPdf.
*
* ConvertMdToPdf is free software : you can redistribute it and / or modify
* it under the terms of the GNU Lesser General Public License as published by
* the Free Software Foundation, either version 3 of the License, or
* (at your option) any later version.
*
* ConvertMdToPdf is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU Lesser General Public License
* along with ConvertMdToPdf.  If not, see <http: *www.gnu.org / licenses / >.
*/

#include "stdafx.h"

#define	NUMELEMENTS(x)	(sizeof(x) / sizeof(x[0]))

enum EParamKeyTypes
{
	PARAM_KEY_MDFILE = 0,
	PARAM_KEY_PDFFILE,
	PARAM_KEY_DEBUG,
	PARAM_KEY_CSS,
	PARAM_KEY_HEADER,
	PARAM_KEY_FOOTER,
};

static const TCHAR* paramKeys[] =
{
	_T("-in"),
	_T("-out"),
	_T("-d"),
	_T("-css"),
	_T("-head"),
	_T("-foot"),
};

#ifdef _UNICODE
#define tstring wstring
#else
#define tstring string
#endif

void PrintVersion()
{
	_tprintf(_T("#############################################################################\n"));
	_tprintf(_T("#  ConvertMdToPdf 0.1\n"));
	_tprintf(_T("#  \n"));
	_tprintf(_T("#  Uses: \n"));
	int maj = 0, min = 0, rev = 0;
	hoedown_version(&maj, &min, &rev);
	printf("#  Hoedown version %d.%d.%d\n", maj, min, rev);
	printf("#  WKHTML version %s\n", wkhtmltopdf_version());

	_tprintf(_T("#  Usage:\n"));
	_tprintf(_T("#  ConvertMdToPdf -in inputfile.md -out outputfile.pdf [-css format.css] [-d] [-head header.htm] [-foot footer.htm]\n"));
	_tprintf(_T("#############################################################################\n"));
}


void progress_changed(wkhtmltopdf_converter * c, int p) {
	printf("%3d%%\r", p);
	fflush(stdout);
}

/* Print loading phase information */
void phase_changed(wkhtmltopdf_converter * c) {
	int phase = wkhtmltopdf_current_phase(c);
	printf("%s\n", wkhtmltopdf_phase_description(c, phase));
}

/* Print a message to stderr when an error occurs */
void error(wkhtmltopdf_converter * c, const char * msg) {
	printf("Error: %s\n", msg);
}

/* Print a message to stderr when a warning is issued */
void warning(wkhtmltopdf_converter * c, const char * msg) {
	printf("Warning: %s\n", msg);
}

const char* ToAnsi(const tstring &str)
{
	static string ansiStr;

#ifdef _UNICODE
	size_t size = str.size();
	ansiStr.resize(size);
	WideCharToMultiByte(CP_ACP, 0, str.c_str(), -1, &(ansiStr[0]), (int)size, 0, 0);
#else
	ansiStr = str;
#endif

	return ansiStr.c_str();
}


int _tmain(int argc, _TCHAR* argv[])
{
	tstring inputMdPath, outputPdfPath, outputHtmPath, cssPath, headerHtmlPath, footerHtmlPath;
	bool isDebug = false;
	for (int argi = 1; argi < argc; argi++)
	{
		for (int paramKeyInx = 0; paramKeyInx < NUMELEMENTS(paramKeys); ++paramKeyInx)
		{
			if (0 == _tcsncicmp(argv[argi], paramKeys[paramKeyInx], _tcslen(paramKeys[paramKeyInx])))
			{
				if (paramKeyInx == PARAM_KEY_MDFILE)
				{
					inputMdPath = argv[argi + 1];
					if (0 == outputPdfPath.length())
					{
						outputPdfPath = inputMdPath;
						outputPdfPath += _T(".pdf");
					}
					if (0 == outputHtmPath.length())
					{
						outputHtmPath = inputMdPath;
						outputHtmPath += _T(".htm");
					}

					TCHAR driveStr[_MAX_PATH], dirStr[_MAX_PATH], fileStr[_MAX_PATH], extStr[_MAX_PATH];
					_tsplitpath_s(inputMdPath.c_str(), driveStr, _MAX_PATH, dirStr, _MAX_PATH, fileStr, _MAX_PATH, extStr, _MAX_PATH);
					if (0 == _tcsicmp(extStr, _T("htm")) || 0 == _tcsicmp(extStr, _T("html")))
					{
						outputHtmPath = inputMdPath;
						inputMdPath.clear();
					}
				}
				else if (paramKeyInx == PARAM_KEY_PDFFILE)
				{
					outputPdfPath = argv[argi + 1];
					argi++;

				}
				else if (paramKeyInx == PARAM_KEY_DEBUG)
				{
					isDebug = true;
				}
				else if (paramKeyInx == PARAM_KEY_CSS)
				{
					cssPath = argv[argi + 1];
				}
				else if (paramKeyInx == PARAM_KEY_HEADER)
				{
					headerHtmlPath = argv[argi + 1];
				}
				else if (paramKeyInx == PARAM_KEY_FOOTER)
				{
					footerHtmlPath = argv[argi + 1];
				}
				break;
			}
		}
	}

#ifdef _DEBUG
	isDebug = true;
#endif

	PrintVersion();
	if (!inputMdPath.length() && !outputHtmPath.length())
	{
		printf("No input file specified");
		return 0;
	}

	wprintf(L"Input .md: %s\n", inputMdPath.c_str());
	wprintf(L"Output .pdf: %s\n", outputPdfPath.c_str());
	if (cssPath.length())
		wprintf(L"input .css: %s\n", cssPath.c_str());

	hoedown_buffer *html = 0;
	hoedown_renderer *renderer = 0;
	hoedown_document *document = 0;
	vector<char> htmlFileBuffer;
	if (inputMdPath.length())
	{
		//read the markdown file into a buffer
		vector<char> mdFileBuffer;
		{
			std::ifstream inputMdFile(inputMdPath.c_str(), ios::in | ios::ate);
			if (!inputMdFile) //Always test the file open.
			{
				_tprintf(_T("Unable to open input markdown file %s"), inputMdPath.c_str());
				return 0;
			}
			ifstream::pos_type mdFileSize = inputMdFile.tellg();
			inputMdFile.seekg(0, ios::beg);

			mdFileBuffer.resize(mdFileSize, 0);
			inputMdFile.read(&mdFileBuffer[0], mdFileSize);
		}

		//use hoedown to convert the markdown to html
		unsigned int flags = HOEDOWN_HTML_HARD_WRAP;
		renderer = hoedown_html_renderer_new((hoedown_html_flags)flags, 0);

		unsigned int extensions = HOEDOWN_EXT_TABLES | HOEDOWN_EXT_NO_INTRA_EMPHASIS;
		document = hoedown_document_new(renderer, (hoedown_extensions)extensions, 16);


		html = hoedown_buffer_new(16);
		hoedown_document_render(document, html, (const uint8_t*)&(mdFileBuffer[0]), mdFileBuffer.size());

		if (isDebug && outputHtmPath.length())
		{
			std::ofstream outputHtmFile(outputHtmPath.c_str(), ios::out | ios::trunc);
			if (outputHtmFile) //Always test the file open.
			{
				outputHtmFile.write((const char*)html->data, html->size);
			}
			else
			{
				_tprintf(_T("Unable to open output htm file: %s"), outputHtmPath.c_str());
			}
		}
	}
	else if (outputHtmPath.length())
	{
		std::ifstream inputHtmFile(outputHtmPath.c_str(), ios::in | ios::ate);
		if (!inputHtmFile) //Always test the file open.
		{
			_tprintf(_T("Unable to open input html file %s"), outputHtmPath.c_str());
			return 0;
		}
		ifstream::pos_type htmFileSize = inputHtmFile.tellg();
		inputHtmFile.seekg(0, ios::beg);

		htmlFileBuffer.resize(htmFileSize, 0);
		inputHtmFile.read(&htmlFileBuffer[0], htmFileSize);
	}

	//use wkhtmltopdf to convert the html to pdf

	/* Init wkhtmltopdf in graphics less mode */
	wkhtmltopdf_init(false);

	//printf("Extended: %d\n", wkhtmltopdf_extended_qt());
	

	wkhtmltopdf_global_settings * gs = wkhtmltopdf_create_global_settings();
	
	//wkhtmltopdf_set_global_setting(gs, "out", "output.pdf");
	//wkhtmltopdf_set_global_setting(gs, "load.cookieJar", "myjar.jar");

	//"size.paperSize" The paper size of the output document, e.g. "A4".
	//"size.width" The with of the output document, e.g. "4cm".
	//"size.height" The height of the output document, e.g. "12in".
	//"orientation" The orientation of the output document, must be either "Landscape" or "Portrait".
	//"colorMode" Should the output be printed in color or gray scale, must be either "Color" or "Grayscale"
	//"resolution" Most likely has no effect.
	//"dpi" What dpi should we use when printing, e.g. "80".
	//"pageOffset" A number that is added to all page numbers when printing headers, footers and table of content.
	//"copies" How many copies should we print ? .e.g. "2".
	//"collate" Should the copies be collated ? Must be either "true" or "false".
	//"outline" Should a outline(table of content in the sidebar) be generated and put into the PDF ? Must be either "true" or false". - \b outlineDepth The maximal depth of the outline, e.g. "4".
	//"dumpOutline" If not set to the empty string a XML representation of the outline is dumped to this file.
	//"out" The path of the output file, if "-" output is sent to stdout, if empty the output is stored in a buffer.
	//"documentTitle" The title of the PDF document.
	//"useCompression" Should we use loss less compression when creating the pdf file ? Must be either "true" or "false".
	//"margin.top" Size of the top margin, e.g. "2cm"
	//"margin.bottom" Size of the bottom margin, e.g. "2cm"
	//"margin.left" Size of the left margin, e.g. "2cm"
	//"margin.right" Size of the right margin, e.g. "2cm"
	//"outputFormat" The format of the output document, must be ether "", "pdf" or "ps".
	//"imageDPI" The maximal DPI to use for images in the pdf document.
	//"imageQuality" The jpeg compression factor to use when producing the pdf document, e.g. "92".
	//"load.cookieJar" Path of file used to load and store cookies.

	wkhtmltopdf_object_settings * os = wkhtmltopdf_create_object_settings();

	//"toc.useDottedLines" Should we use a dotted line when creating a table of content ? Must be either "true" or "false".
	//"toc.captionText" The caption to use when creating a table of content.
	//"toc.forwardLinks" Should we create links from the table of content into the actual content ? Must be either "true or "false.
	//"toc.backLinks" Should we link back from the content to this table of content.
	//"toc.indentation" The indentation used for every table of content level, e.g. "2em".
	//"toc.fontScale" How much should we scale down the font for every toc level ? E.g. "0.8"
	//"page" The URL or path of the web page to convert, if "-" input is read from stdin.
	//"header.*" Header specific settings see Header and footer settings.
	//"footer.*" Footer specific settings see Header and footer settings.
		//"header.fontSize The font size to use for the header, e.g. "13"
		//"header.fontName The name of the font to use for the header.e.g. "times"
		//"header.left The string to print in the left part of the header, note that some sequences are replaced in this string, see the wkhtmltopdf manual.
		//"header.center The text to print in the center part of the header.
		//"header.right The text to print in the right part of the header.
		//"header.line Should a line be printed under the header ? Must be either "true" or "false. - \b header.space The amount of space to put between the header and the content, e.g. "1.8". Be aware that if this is to large the header will be printed outside the pdf document. This can be correct with the margin.top setting.
		//"header.htmlUrl Url for a HTML document to use for the header.
	if (headerHtmlPath.length())
	{
		string htmPathStr = ToAnsi(headerHtmlPath);
		printf("Using html header: %s\n", htmPathStr.c_str());
		wkhtmltopdf_set_object_setting(os, "os.header.htmlUrl", htmPathStr.c_str());
	}

	if (footerHtmlPath.length())
	{
		string htmPathStr = ToAnsi(footerHtmlPath);
		printf("Using html footer: %s\n", htmPathStr.c_str());
		wkhtmltopdf_set_object_setting(os, "os.footer.htmlUrl", htmPathStr.c_str());
	}
	else
	{
		wkhtmltopdf_set_object_setting(os, "os.footer.center", "Page: [page] of [topage]");
	}
	//"useExternalLinks" Should external links in the HTML document be converted into external pdf links ? Must be either "true" or "false.
	//"useLocalLinks" Should internal links in the HTML document be converted into pdf references ? Must be either "true" or "false"
	//"replacements" TODO
	//"produceForms" Should we turn HTML forms into PDF forms ? Must be either "true" or file".
	//"load.*" Page specific settings related to loading content, see Object Specific loading settings.
	//"web.*" See Web page specific settings.
		//"web.background Should we print the background ? Must be either "true" or "false".
		//"web.loadImages" Should we load images ? Must be either "true" or "false".
		//"web.enableJavascript" Should we enable javascript ? Must be either "true" or "false".
		//"web.enableIntelligentShrinking" Should we enable intelligent shrinkng to fit more content on one page ? Must be either "true" or "false".Has no effect for wkhtmltoimage.
		//"web.minimumFontSize" The minimum font size allowed.E.g. "9"
		//"web.printMediaType" Should the content be printed using the print media type instead of the screen media type.Must be either "true" or "false".Has no effect for wkhtmltoimage.
		//"web.defaultEncoding" What encoding should we guess content is using if they do not specify it propperly ? E.g. "utf-8"
	if (cssPath.length())
	{
		string cssPathStr = ToAnsi(cssPath);
		wkhtmltopdf_set_object_setting(os, "web.userStyleSheet", cssPathStr.c_str()); // Url er path to a user specified style sheet.
	}
	//"web.enablePlugins" Should we enable NS plugins, must be either "true" or "false".Enabling this will have limited success.
	//"includeInOutline" Should the sections from this document be included in the outline and table of content ?
	//"pagesCount" Should we count the pages of this document, in the counter used for TOC, headers and footers ?
	//"tocXsl" If not empty this object is a table of content object, "page" is ignored and this xsl style sheet is used to convert the outline XML into a table of content.
	
	
	wkhtmltopdf_converter * c = wkhtmltopdf_create_converter(gs);

	/* Call the progress_changed function when progress changes */
	wkhtmltopdf_set_progress_changed_callback(c, progress_changed);

	/* Call the phase _changed function when the phase changes */
	wkhtmltopdf_set_phase_changed_callback(c, phase_changed);

	/* Call the error function when an error occurs */
	wkhtmltopdf_set_error_callback(c, error);

	/* Call the warning function when a warning is issued */
	wkhtmltopdf_set_warning_callback(c, warning);

	if (html)
		wkhtmltopdf_add_object(c, os, (const char*)html->data);
	else if (htmlFileBuffer.size())
		wkhtmltopdf_add_object(c, os, (const char*)&htmlFileBuffer[0]);
	
	if (!wkhtmltopdf_convert(c))
		printf("Conversion failed!\n");

	//wkhtmltopdf_get_output(wkhtmltopdf_converter * converter, const unsigned char **);

	const unsigned char* htmlOutputData = 0;
	long htmlDataSize = wkhtmltopdf_get_output(c, &htmlOutputData);



	std::ofstream outputPdfFile(outputPdfPath.c_str(), ios::out | ios::binary);
	if (!outputPdfFile) //Always test the file open.
	{
		_tprintf(_T("Unable to open output pdf file: %s"), outputPdfPath.c_str());
		return 0;
	}

	outputPdfFile.write((const char*)htmlOutputData, htmlDataSize);

	wkhtmltopdf_destroy_converter(c);
	wkhtmltopdf_deinit();

	if (document)
		hoedown_document_free(document);
	if (renderer)
		hoedown_html_renderer_free(renderer);
	if (html)
		hoedown_buffer_free(html);


	return 1;
}

