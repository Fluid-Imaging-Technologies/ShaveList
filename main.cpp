/*
 * $Id: main.cpp,v 1.4 2009/04/01 18:29:38 scott Exp $
 *
 */

#include <stdio.h>
#include "main.h"

void set_desription(HWND hDlg);
bool choose_src_list(HWND hDlg);
bool run(HWND hDlg);
int count_lines_in_file(HWND hDlg, const char *filename);
bool shave(HWND hDlg, int line_count, int max_particles, FILE *fin, FILE *fout);
bool copy_context(FILE *fin, FILE *fout);
int* generate_keep_list(int *num_to_keep, int total, unsigned int seed);
BOOL CALLBACK DlgProc(HWND hDlg, UINT msg, WPARAM wParam, LPARAM lParam);

HINSTANCE ghInstance;

#define NUM_PRIMES 22

int prime_list[NUM_PRIMES] = 
	{ 4177, 3691, 3137, 2383, 1741, 
	  1061, 743, 521, 419, 271, 
	  199, 127, 83, 61, 43, 
	  31, 23, 19, 13, 11, 
	  7, 3 };


/*
  =======================================================================================
  =======================================================================================
*/
int WINAPI WinMain(HINSTANCE hInst, HINSTANCE hPrevInst, LPSTR lpCmdLine, int nCmdShow)
{
	MSG msg;
	int status;
	HWND hDlg;

	ghInstance = hInst;

	InitCommonControls();

	hDlg = CreateDialog(ghInstance, MAKEINTRESOURCE(IDD_SHAVE_LIST_DLG), NULL, DlgProc);

	if (hDlg) {
		ShowWindow(hDlg, 1);

		while ((status = GetMessage(&msg, 0, 0, 0)) != 0) {
			if (status == -1) {
				return -1;
			}
			else if (!IsDialogMessage (hDlg, &msg)) {
				TranslateMessage(&msg);
				DispatchMessage(&msg);
			}
		}
	}

	
    return (int) msg.wParam;
}

/*
  =======================================================================================
  =======================================================================================
*/
void set_description(HWND hDlg)
{
	char buff[512];

	strncpy_s(buff, sizeof(buff), "The output list file will be written to the same directory", _TRUNCATE);
	strncat_s(buff, sizeof(buff), " as the original because it will still be\r\n", _TRUNCATE);
	strncat_s(buff, sizeof(buff), " referencing the same image files.\r\n", _TRUNCATE);
	strncat_s(buff, sizeof(buff), "Change the random number seed to get different results", _TRUNCATE);
	strncat_s(buff, sizeof(buff), " for the same list file.", _TRUNCATE);

	SetDlgItemText(hDlg, IDC_DESCRIPTION, buff);

	sprintf_s(buff, sizeof(buff), "ShaveList %d.%d", MAJOR_VERSION, MINOR_VERSION);
	SetWindowText(hDlg, buff);
}

/*
  =======================================================================================
  =======================================================================================
*/
BOOL CALLBACK DlgProc(HWND hDlg, UINT msg, WPARAM wParam, LPARAM lParam)
{
	switch (msg) 
	{
	case WM_INITDIALOG:
		SetDlgItemInt(hDlg, IDC_MAX_PARTICLES, 1000, TRUE);
		SendMessage(GetDlgItem(hDlg, IDC_RAND_SEED), EM_SETLIMITTEXT, 8, 0);
		SetDlgItemInt(hDlg, IDC_RAND_SEED, 1, FALSE);
		set_description(hDlg);
		return TRUE;

	case WM_COMMAND:
		switch (LOWORD(wParam)) 
		{
		case IDC_CHOOSE_LIST:
			choose_src_list(hDlg);
			break;

		case ID_RUN:
			if (run(hDlg)) {
				MessageBox(hDlg, "New list file created.", "Success", MB_OK);
			}

			break;

		case IDCANCEL:
			DestroyWindow(hDlg);
			break;
		}

		return TRUE;

	case WM_DESTROY:
		PostQuitMessage(0);
		return TRUE;		
	}

	return FALSE;
}

/*
  =======================================================================================
  =======================================================================================
*/
bool choose_src_list(HWND hDlg)
{
	OPENFILENAME ofn;
	char listFile[MAX_PATH], title[64];
	char dst_name[MAX_PATH];

	memset(listFile, 0, MAX_PATH);

	strncpy_s(title, sizeof(title), "Choose List File", _TRUNCATE);

	memset(&ofn, 0, sizeof(OPENFILENAME));
	ofn.lStructSize = sizeof(OPENFILENAME);
	ofn.hwndOwner = hDlg;
	ofn.hInstance = ghInstance;
	ofn.lpstrFilter = "List File (*.lst)\0*.lst\0";
	ofn.lpstrFile = listFile;
	ofn.nMaxFile = MAX_PATH - 1;
	ofn.lpstrFileTitle = NULL;
	ofn.nMaxFileTitle = 0;
	ofn.lpstrTitle = title;
	ofn.lpstrInitialDir = NULL;
	ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;

	if (!GetOpenFileName(&ofn)) {
		return false;
	}

	SetDlgItemText(hDlg, IDC_ORIGINAL_LIST, listFile);

	memset(dst_name, 0, sizeof(dst_name));

	// put in a default
	char *p = strrchr(listFile, '\\');

	if (p) {
		strncpy_s(dst_name, sizeof(dst_name), p + 1, _TRUNCATE);

		p = strrchr(dst_name, '.');

		if (p) {
			*p = 0;
			strncat_s(dst_name, sizeof(dst_name), "_shaved.lst", _TRUNCATE);
			SetDlgItemText(hDlg, IDC_OUTPUT_LIST, dst_name);
		}
	}

	return true;
}

/*
  =======================================================================================
  =======================================================================================
*/
bool run(HWND hDlg)
{
	char src[MAX_PATH], dst[MAX_PATH], dst_name[MAX_PATH], error[2 * MAX_PATH];
	char *p;
	FILE *fin, *fout;
	int max_particles, line_count;
	bool result;
	
	max_particles = GetDlgItemInt(hDlg, IDC_MAX_PARTICLES, NULL, FALSE);

	if (max_particles == 0) {
		MessageBox(hDlg, "Zero max particles is not acceptable.", "Error", MB_OK);
		return false;
	}

	memset(src, 0, sizeof(src));
	GetDlgItemText(hDlg, IDC_ORIGINAL_LIST, src, MAX_PATH - 1);

	if (!file_exists(src)) {
		return false;
	}

	memset(dst_name, 0, sizeof(dst_name));
	GetDlgItemText(hDlg, IDC_OUTPUT_LIST, dst_name, sizeof(dst_name));

	if (strlen(dst_name) == 0) {
		MessageBox(hDlg, "Need an output file name.", "Error", MB_OK);
		return false;
	}

	strncpy_s(dst, sizeof(dst), src, _TRUNCATE);

	p = strrchr(dst, '\\');

	if (!p) {
		MessageBox(hDlg, "Can't find a path delimiter in the source file!", "Error", MB_OK);
		return false;
	}

	p++;
	*p = 0;
	strncat_s(dst, sizeof(dst), dst_name, _TRUNCATE);

	if (!strcmp(src, dst)) {
		MessageBox(hDlg, "Sorry, not going to overwrite the original list file.", "No Clobbering Originals", MB_OK);
		return false;
	}

	if (file_exists(dst)) {
		sprintf_s(error, sizeof(error), "The output file\n%s\nalready exists.\n\nOverwrite?", dst);

		if (IDNO == MessageBox(hDlg, error, "Overwrite Existing File", MB_YESNO)) {
			return false;
		}
	}
	
	line_count = count_lines_in_file(hDlg, src);

	if (line_count < 0) {
		// error already shown
		return false;
	}
	else if (line_count < 2) {
		MessageBox(hDlg, "Source file line count is too small to believe", "Error", MB_OK);
		return false;
	}

	if (fopen_s(&fin, src, "rb")) {
		MessageBox(hDlg, "Error opening original file.", "Error", MB_OK);
		return false;
	}

	if (fopen_s(&fout, dst, "wb")) {
		fclose(fin);
		MessageBox(hDlg, "Error opening output file.", "Error", MB_OK);
		return false;
	}

	SetCursor(LoadCursor(NULL, IDC_WAIT));

	result = shave(hDlg, line_count, max_particles, fin, fout);
	
	fclose(fin);
	fclose(fout);

	SetCursor(LoadCursor(NULL, IDC_ARROW));

	if (result) {
		p = strrchr(src, '.');

		if (!p) {
			MessageBox(hDlg, "Internal error, source context", "Error Copying Context", MB_OK);
			return false;
		}
		
		*p = 0;
		strncat_s(src, MAX_PATH, ".ctx", _TRUNCATE);

		if (!file_exists(src)) {
			MessageBox(hDlg, "Failed to find context file for original.", "Error Copying Context", MB_OK);
			return false;
		}

		p = strrchr(dst, '.');

		if (!p) {
			MessageBox(hDlg, "Internal error, dest context", "Error Copying Context", MB_OK);
			return false;
		}
			
		*p = 0;
		strncat_s(dst, MAX_PATH, ".ctx", _TRUNCATE);

		if (fopen_s(&fin, src, "rb")) {
			MessageBox(hDlg, "Error opening original context file.", "Error", MB_OK);
			return false;
		}

		if (fopen_s(&fout, dst, "wb")) {
			fclose(fin);
			MessageBox(hDlg, "Error opening output context file.", "Error", MB_OK);
			return false;
		}

		result = copy_context(fin, fout);

		fclose(fin);
		fclose(fout);

		if (!result) {
			MessageBox(hDlg, "Error copying context file.", "Error", MB_OK);
		}

	}

	return result;
}

/*
  =======================================================================================
  =======================================================================================
*/
bool copy_context(FILE *fin, FILE *fout)
{
	char buff[1024];

	memset(buff, 0, sizeof(buff));

	while (!feof(fin)) {
		if (fgets(buff, sizeof(buff), fin)) {
			if (fputs(buff, fout) < 0) {
				return false;
			}
		}
		else if (ferror(fin)) {
			return false;
		}
	}

	return true;
}

/*
  =======================================================================================
  =======================================================================================
*/
bool shave_v2_v16(HWND hDlg, int version, int line_count, int max_particles, FILE *fin, FILE *fout)
{
	char line[2048];
	int num_particles, num_to_keep;
	int *keep_list, counter, written;
	unsigned int seed;

	memset(line, 0, sizeof(line));

	if (!fgets(line, 32, fin)) {
		MessageBox(hDlg, "Error reading particle count from list file.",
			"Error", MB_OK);
		return false;
	}

	num_particles = atoi(line);
	num_to_keep = max_particles;

	seed = GetDlgItemInt(hDlg, IDC_RAND_SEED, NULL, FALSE);

	if (seed == 0) {
		seed = 1;
	}

	keep_list = generate_keep_list(&num_to_keep, num_particles, seed);

	if (!keep_list) {
		MessageBox(hDlg, "Internal error allocating memory.", "Error", MB_OK);
		return false;
	}

	sprintf_s(line, sizeof(line), "%d\n%d\n", version, num_to_keep);
	
	if (fputs(line, fout) < 0) {
		MessageBox(hDlg, "Error writing output file", "Error", MB_OK);
		return false;
	}

	counter = 0;
	written = 0;

	while (!feof(fin)) {
		if (!fgets(line, sizeof(line), fin)) {
			if (ferror(fin)) {
				MessageBox(hDlg, "Error reading input file", "Error", MB_OK);
				return false;
			}
		}

		if (keep_list[counter++]) {
			if (fputs(line, fout) < 0) {
				MessageBox(hDlg, "Error writing output file", "Error", MB_OK);
				return false;
			}

			if (++written >= num_to_keep) {
				break;
			}
		}
	}

	delete [] keep_list;

	return true;
}

/*
  =======================================================================================
  =======================================================================================
*/
int get_field_count(char *field_count_line)
{
	char *p;

	p = strrchr(field_count_line, '|');

	if (!p)
		return 0;

	p++;

	return atoi(p);
}

/*
  =======================================================================================
  =======================================================================================
*/
bool shave_v17(HWND hDlg, int version, int line_count, int max_particles, FILE *fin, FILE *fout)
{
	char line[2048];
	int num_particles, num_to_keep, num_fields;
	int *keep_list, counter, written;
	unsigned int seed;

	memset(line, 0, sizeof(line));

	if (!fgets(line, 32, fin)) {
		MessageBox(hDlg, "Error reading field count from list file.",
			"Error", MB_OK);
		return false;
	}

	num_fields = get_field_count(line);

	if (num_fields < 10) {
		MessageBox(hDlg, "Suspiciously low field count in list file. Not processing.", "Error", MB_OK);
		return false;
	}
	
	if (num_fields + 2 >= line_count) {
		MessageBox(hDlg, "More fields then lines in file. Not processing.", "Error", MB_OK);
		return false;
	}

	num_particles = line_count - (num_fields + 2);

	num_to_keep = max_particles;

	seed = GetDlgItemInt(hDlg, IDC_RAND_SEED, NULL, FALSE);

	if (seed == 0) {
		seed = 1;
	}

	keep_list = generate_keep_list(&num_to_keep, num_particles, seed);

	if (!keep_list) {
		MessageBox(hDlg, "Internal error allocating memory.", "Error", MB_OK);
		return false;
	}

	sprintf_s(line, sizeof(line), "%03d\nnum-fields|%d\n", version, num_fields);
	
	if (fputs(line, fout) < 0) {
		MessageBox(hDlg, "Error writing output file", "Error", MB_OK);
		return false;
	}

	// write out the fields
	for (int i = 0; i < num_fields; i++) {
		if (!fgets(line, sizeof(line), fin)) {
			if (ferror(fin)) {
				MessageBox(hDlg, "Error reading input file", "Error", MB_OK);
				return false;
			}
		}

		if (fputs(line, fout) < 0) {
			MessageBox(hDlg, "Error writing output file", "Error", MB_OK);
			return false;
		}
	}

	counter = 0;
	written = 0;

	while (!feof(fin)) {
		if (!fgets(line, sizeof(line), fin)) {
			if (ferror(fin)) {
				MessageBox(hDlg, "Error reading input file", "Error", MB_OK);
				return false;
			}
		}

		if (keep_list[counter++]) {
			if (fputs(line, fout) < 0) {
				MessageBox(hDlg, "Error writing output file", "Error", MB_OK);
				return false;
			}

			if (++written >= num_to_keep) {
				break;
			}
		}
	}

	delete [] keep_list;

	return true;
}

/*
  =======================================================================================
  =======================================================================================
*/
bool shave(HWND hDlg, int line_count, int max_particles, FILE *fin, FILE *fout)
{
	char line[64];
	int version;

	memset(line, 0, sizeof(line));

	if (!fgets(line, 32, fin)) {
		MessageBox(hDlg, "Error reading list file version.", "Error", MB_OK);
		return false;
	}

	version = atoi(line);

	if (version < 2 || version > 17) {
		MessageBox(hDlg, "ShaveList currently only supports list file versions 2 - 17.", 
			"Unsupported Version", MB_OK);
		return false;
	}

	if (version == 17)
		return shave_v17(hDlg, version, line_count, max_particles, fin, fout);
	else
		return shave_v2_v16(hDlg, version, line_count, max_particles, fin, fout);
}

/*
  =======================================================================================
  =======================================================================================
*/
int* generate_keep_list(int *num_to_keep, int total, unsigned int seed)
{
	int *keepers, n, i, j;
	double keeper_cutoff, r;
	bool even;

	if (total < 1) {
		return NULL;
	}

	srand(seed);
	// eat a couple of iterations or we always get id=1 particle
	r = rand() * rand();

	// don't let this chomping get optimized away
	if (r == 0.0) {
		return NULL;
	}

	if (*num_to_keep >= total) {
		*num_to_keep = total;		
	}
	else {
		keeper_cutoff = (double)(*num_to_keep) / (double) total;
	}

	keepers = new int[total];
			
	if (!keepers) {
		return NULL;
	}

	if (*num_to_keep == total) {
		for (i = 0; i < total; i++) {
			keepers[i] = 1;
		}

		return keepers;
	}

	memset(keepers, 0, total * sizeof(int));

	n = 0;
	j = 0;

	while (n < *num_to_keep && j < 10) {
		for (i = 0; i < total; i++) {
			if (!keepers[i]) {
				r = rand() / (double) RAND_MAX;

				if (r < keeper_cutoff) {
					keepers[i] = 1;
						
					if (++n == *num_to_keep) {
						break;
					}
				}
			}
		}

		// don't keep spinning forever
		j++;
	}

	// in case we need a few more
	while (n < *num_to_keep) {
		r = rand() / (double) RAND_MAX;

		even = (r > 0.5);

		for (j = 0; j < NUM_PRIMES; j++) {
			if (even) {
				for (i = prime_list[j]; i < total; i += prime_list[j]) {
					if (!keepers[i]) {
						keepers[i] = 1;

						if (++n == *num_to_keep) {
							break;
						}
					}
				}
			}
			else {
				for (i = total - prime_list[j]; i >= 0; i -= prime_list[j]) {
					if (!keepers[i]) {
						keepers[i] = 1;

						if (++n == *num_to_keep) {
							break;
						}
					}
				}		
			}

			even = !even;
		}
	}

	*num_to_keep = n;

	return keepers;
}

/*
  =======================================================================================
  =======================================================================================
*/
bool file_exists(const char *filename)
{
	WIN32_FILE_ATTRIBUTE_DATA fileInfo;

	if (filename && *filename) {
		if (!GetFileAttributesEx(filename, GetFileExInfoStandard, &fileInfo)) {
			return false;
		}

		return true;
	}

	return false;
}

/*
  =======================================================================================
  Only use for files less then 4GB
  =======================================================================================
*/
unsigned long get_file_size(const char *filename)
{
	WIN32_FILE_ATTRIBUTE_DATA fileInfo;

	if (filename && *filename) {
		if (GetFileAttributesEx(filename, GetFileExInfoStandard, &fileInfo)) {
			return fileInfo.nFileSizeLow;
		}
	}

	return 0;
}

/*
  =======================================================================================
  =======================================================================================
*/
int count_lines_in_file(HWND hDlg, const char *src)
{
	int line_count;
	FILE *fin;
	char *line;

	line = new char[2048];

	if (!line) {
		MessageBox(hDlg, "Error allocating buffer memory.", "Memory", MB_OK);
		return -1;
	}

	memset(line, 0, 2048);

	if (fopen_s(&fin, src, "rb")) {
		MessageBox(hDlg, "Error opening original file.", "Error", MB_OK);
		delete [] line;
		return -1;
	}

	line_count = 0;

	while (!feof(fin)) {
		if (!fgets(line, 2040, fin)) {
			if (ferror(fin)) {
				MessageBox(hDlg, "Error reading input file", "Error", MB_OK);
				line_count = -1;
				break;
			}
		}

		line_count++;
	}

	delete [] line;

	fclose(fin);

	return line_count;
}