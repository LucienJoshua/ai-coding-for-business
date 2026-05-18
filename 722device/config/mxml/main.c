#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "mxml.h"

static void show_text(const char* text);

int main(int argc, char** argv)
{

	FILE*			fp = NULL;
	mxml_node_t*	root = NULL;
	mxml_node_t*	node = NULL;

	fp = fopen("./test.xml", "r");
	if (NULL == fp)
	{
		printf("Open the text.xml file fail.\n");
		getchar();
		exit(0);
	}

	root = mxmlLoadFile(NULL, NULL, fp);
	if (NULL == root)
	{
		printf("mxml load file fail.\n");
		goto __FINISH;
	}
#if 0
	const char* root_text = mxmlGetElement(root);
	if (root_text)
	{
		printf("root text:%s.\n", root_text);
	}
	else
	{
		printf("root text is empty.\n");
	}
#endif

	node = mxmlGetFirstChild(root);
	const char* element = mxmlGetElement(node);
	const char* text = mxmlGetText(node, NULL);
	show_text(element);
	show_text(text);

	while (node != NULL)
	{
		
	}

__FINISH:
	if (root)
	{
		mxmlDelete(root);
	}
	if (fp)
	{
		fclose(fp);
	}
	getchar();
	return 0;
}

static void show_text(const char* text)
{
	if (text)
	{
		printf("%s.\n", text);
	}
}
