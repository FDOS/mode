#include <stdio.h>

int main(int argc, char **argv)
{
  FILE *in, *out;
  int col;
  int c;

  if (argc < 4)
  {
    fprintf(stderr,
            "Usage: bin2c <input bin file> <output h file> <array name>\n");
    return 1;
  }

  if ((in = fopen(argv[1], "rb")) == NULL)
  {
    fprintf(stderr, "Cannot open input file (%s).\n", argv[1]);
    return 1;
  }

  if ((out = fopen(argv[2], "wt")) == NULL)
  {
    fprintf(stderr, "Cannot open output file (%s).\n", argv[2]);
    return 1;
  }

  col = 0;

  fprintf(out, "/* File generated by bin2c. Do not edit this [%s] file. */\n",
    argv[2]);
  fprintf(out, "/* Instead, update the [%s] file and run bin2c again. */\n\n",
    argv[1]);

  fprintf(out, "unsigned char %s[] = {\n  ", argv[3]);

  while ((c = fgetc(in)) != EOF)
  {
    if (col)
    {
      fprintf(out, ", ");
    }
    if (col >= 8)
    {
      fprintf(out, "\n  ");
      col = 0;
    }
    fprintf(out, "0x%02X", c);
    col++;
  }

  fprintf(out, "\n};\n");
  fclose(in);
  fclose(out);

  return 0;
}
