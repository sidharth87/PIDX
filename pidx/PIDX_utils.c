/*****************************************************
 **  PIDX Parallel I/O Library                      **
 **  Copyright (c) 2010-2014 University of Utah     **
 **  Scientific Computing and Imaging Institute     **
 **  72 S Central Campus Drive, Room 3750           **
 **  Salt Lake City, UT 84112                       **
 **                                                 **
 **  PIDX is licensed under the Creative Commons    **
 **  Attribution-NonCommercial-NoDerivatives 4.0    **
 **  International License. See LICENSE.md.         **
 **                                                 **
 **  For information about this project see:        **
 **  http://www.cedmav.com/pidx                     **
 **  or contact: pascucci@sci.utah.edu              **
 **  For support: PIDX-support@visus.net            **
 **                                                 **
 *****************************************************/

#include "PIDX_inc.h"

unsigned int getNumBits ( unsigned int v )
{
  return (unsigned int)floor((log2(v))) + 1;
}

unsigned long long getPowerOf2(int x)
{
  /*  find the power of 2 of an integer value (example 5->8) */
  int n = 1;
  while (n < x) n <<= 1;
  return n;
}

unsigned int getLevelFromBlock (long long block, int bits_per_block)
{
  if (block == 0)
    return 0;
  else
    return (unsigned int)floor((log2(getPowerOf2(block)))) + bits_per_block;
  
  return 0;
}

unsigned int getLeveL (unsigned long long index)
{
  if (index)
    return (unsigned int)floor((log2(index))) + 1;
  /* To deal with index 0 - level 0 */
  return 0;
}

int isValidBox(int** box)
{
  int D;
  for(D = 0 ; D < PIDX_MAX_DIMENSIONS ; D++)
  {
    //printf("VFY: %d %d\n", box[0][D], box[1][D]);
    if (! (box[0][D]>=0 && box[0][D]<=box[1][D]))
      return 0;
  }

  return 1;
}

void Deinterleave(const char* bitmask, int maxh, unsigned long long zaddress, int* point)
{
  //Z deinterleave (see papers!)  
  int n = 0, bit;    
  int* cnt_point = (int*)malloc(PIDX_MAX_DIMENSIONS*sizeof(int));
  memset(cnt_point, 0, PIDX_MAX_DIMENSIONS*sizeof(int));

  for (;zaddress;zaddress >>= 1,++n,maxh--) 
  {
    bit=bitmask[maxh];
    point[bit] |= (zaddress & 1) << cnt_point[bit];
    ++cnt_point[bit];
  }
  free(cnt_point);
  cnt_point = 0;
}

unsigned long long ZBitmask(const char* bitmask,int maxh)
{
  return ((unsigned long long)1)<<maxh;
}

unsigned long long ZStart(const char* bitmask,int maxh,int BlockingH)
{
  if (!BlockingH) 
    return 0;
  assert(BlockingH>=1 && BlockingH<=maxh);
  return ((unsigned long long)1)<<(maxh-BlockingH);
}

unsigned long long ZEnd(const char* bitmask,int maxh,int BlockingH)
{
  if (!BlockingH) 
    return 0;
  assert(BlockingH>=1 && BlockingH<=maxh);
  return (ZBitmask(bitmask,maxh)-1)-(ZStart(bitmask,maxh,BlockingH)-1);
}

void ZDelta(const char* bitmask, int maxh, int BlockingH, int* point)
{
  int K, bit;
  for(K = 0; K < PIDX_MAX_DIMENSIONS; K++)
    point[K] = 1;
  
  if (!BlockingH) return;
  assert(BlockingH>=1 && BlockingH<=maxh);
  for (K=maxh;K>=BlockingH;K--)
  {
    bit=bitmask[K];
    point[bit] <<= 1;
  }
}

void GetBoxIntersection(int** inputBox1, int** inputBox2, int** outputBox)
{
  //returns the intersection of two boxes
  int i;      
  for(i = 0 ; i < PIDX_MAX_DIMENSIONS ; i++)
  {
    outputBox[0][i]=Max2ab(inputBox1[0][i], inputBox2[0][i]);
    outputBox[1][i]=Min2ab(inputBox1[1][i], inputBox2[1][i]);
  }
}

int** AlignEx(int** box, int* p0, int* delta)
{
  int mod,i;
  if (!isValidBox(box))
    return box;
  for( i = 0 ; i < PIDX_MAX_DIMENSIONS ; i++)
  {
    mod=(box[0][i]-p0[i]) % delta[i]; 
    if (mod) box[0][i]+=(delta[i]-mod);
    mod=(box[1][i]-p0[i]) % delta[i]; 
    if (mod) box[1][i]-=mod;
  }
  return box;
}

void revstr(char* str)
{
  long long i;
  char cpstr[strlen(str)+1];
  for(i=0; i < strlen(str); i++)
    cpstr[i] = str[strlen(str)-i-1];
  
  cpstr[i] = '\0';
  strcpy(str, cpstr);
}

// IMPORTANT Valerio, Giorgio and Cameron need to argue about this function
// Cna make a differnec in performance
void GuessBitmaskPattern(char* _bits, PointND dims)
{
  int D,N,ordered;
  int dim = 1;
  char* p=_bits;
	      
  PointND id,sorted_id;
    
  *p++='V';

  For(D)
  {
    PGET(dims,D)=( int)getPowerOf2(PGET(dims,D));
    PGET(id,D)=D;
  }

  //order is ASC order (from smaller dimension to bigger)
  for (ordered=0;!ordered;)
  {
    ordered=1;
    OffsetFor(D,0,-1)
    {
      int ref0=PGET(id,D  ),dim0=PGET(dims,ref0);
      int ref1=PGET(id,D+1),dim1=PGET(dims,ref1);
      if (!(dim0<dim1 || (dim0==dim1 && ref0<ref1)))
      {
	int _temp=PGET(id,D);
	PGET(id,D)=PGET(id,D+1);
	PGET(id,D+1)=_temp;
	ordered=0;
      }
    }
  }
  
  For(D)
  {
    //order in DESC order
    for (ordered=0,sorted_id=id;!ordered;)
    {
      ordered=1;
      OffsetFor(N,D,-1)
      {
	if (PGET(sorted_id,N+0)<PGET(sorted_id,N+1)) 
	{
	  int _temp=PGET(sorted_id,N);
	  PGET(sorted_id,N)=PGET(sorted_id,N+1);
	  PGET(sorted_id,N+1)=_temp;
	  ordered=0;
	}
      }
    }
    //while dim is not consumed
    for (;dim<PGET(dims,PGET(id,D));dim<<=1)
    {
      OffsetFor(N,D,0)
      {
	*p++='0'+PGET(sorted_id,N);	
      }
    }
  }
  *p++=0;
  revstr(_bits+1);
  //strrev(_bits+1)
}

static void freeBox(int** box)
{
  free(box[0]);
  box[0] = 0;
  free(box[1]);
  box[1] = 0;
  
  free(box);
  box = 0;
}

void Align(int maxh, int H, const char* bitmask, int** userBox, int** a_offset, int** a_count)
{
  int** alignedBox;
  int* h_delta;
  int** h_box;
  
  if (!isValidBox(userBox))
    return;

  if (!(H>=0 && H<=maxh))
    return;

  h_delta = malloc(PIDX_MAX_DIMENSIONS *sizeof(int));
  memset(h_delta, 0, PIDX_MAX_DIMENSIONS *sizeof(int));
  
  ZDelta(bitmask,maxh,H, h_delta);
  
  //ZBox
  h_box = (int**)malloc(2* sizeof(int*));
  memset(h_box, 0, 2* sizeof(int*));
  
  h_box[0] = (int*)malloc(PIDX_MAX_DIMENSIONS * sizeof(int));
  h_box[1] = (int*)malloc(PIDX_MAX_DIMENSIONS * sizeof(int));
  memset(h_box[0], 0, PIDX_MAX_DIMENSIONS * sizeof(int));
  memset(h_box[1], 0, PIDX_MAX_DIMENSIONS * sizeof(int));
  
  if(!H)
  {
    h_box[0][0] = h_box[0][1] = h_box[0][2] = h_box[0][3] = h_box[0][4] = 0;
    h_box[1][0] = h_box[1][1] = h_box[1][2] = h_box[1][3] = h_box[1][4] = 0;
  }
  else
  {
    assert(H>=1 && H<=maxh);
    Deinterleave(bitmask,maxh,ZStart(bitmask,maxh,H), h_box[0]);
    Deinterleave(bitmask,maxh,ZEnd  (bitmask,maxh,H), h_box[1]);
  }
  
  alignedBox = (int**)malloc(2* sizeof(int*));
  memset(alignedBox, 0, 2* sizeof(int*));
  alignedBox[0] = (int*)malloc(PIDX_MAX_DIMENSIONS * sizeof(int));
  alignedBox[1] = (int*)malloc(PIDX_MAX_DIMENSIONS * sizeof(int));
  memset(alignedBox[0], 0, PIDX_MAX_DIMENSIONS * sizeof(int));
  memset(alignedBox[1], 0, PIDX_MAX_DIMENSIONS * sizeof(int));
  
  //calculate intersection of the query with current H box
  GetBoxIntersection(userBox, h_box, alignedBox);
  
  //the box is not valid
  if (!isValidBox(alignedBox))
  {
    freeBox(h_box);
    freeBox(alignedBox);
    free(h_delta);
    return;
  }

  alignedBox = AlignEx(alignedBox, h_box[0], h_delta);
  
  //invalid box
  if (!isValidBox(alignedBox))
  {  
    freeBox(h_box);
    freeBox(alignedBox);
    free(h_delta);
    return;
  }

  memcpy(a_offset[H], alignedBox[0], PIDX_MAX_DIMENSIONS * sizeof(int) );
  memcpy(a_count[H], alignedBox[1], PIDX_MAX_DIMENSIONS * sizeof(int) );
  
  freeBox(h_box);
  freeBox(alignedBox);
  free(h_delta);
  return;
}

int RegExBitmaskBit(const char* bitmask_pattern,int N)
{
  const char *OpenRegEx;
  int S, L;
  assert(bitmask_pattern[0]=='V');

  if (!N) 
    return bitmask_pattern[0];
  
  if ((OpenRegEx=strchr(bitmask_pattern,'{')))
  {
    S = 1+OpenRegEx-bitmask_pattern;
    L = strchr(bitmask_pattern,'}')-bitmask_pattern-S;

    if ((N+1)<S)
      return bitmask_pattern[N]-'0';
    else
      return bitmask_pattern[S+((N+1-S)%L)]-'0';
  }
  return bitmask_pattern[N]-'0';
}

void Hz_to_xyz(const char* bitmask,  int maxh, long long hzaddress, long long* xyz)
{
  long long lastbitmask=((long long)1)<<maxh;
  
  hzaddress <<= 1;
  hzaddress  |= 1;
  while ((lastbitmask & hzaddress) == 0) hzaddress <<= 1;
    hzaddress &= lastbitmask - 1;
  
  PointND cnt;
  PointND p  ;
  int n = 0;

  memset(&cnt,0,sizeof(PointND));
  memset(&p  ,0,sizeof(PointND));

  for (;hzaddress; hzaddress >>= 1,++n, maxh--) 
  {
    int bit= bitmask[maxh];
    PGET(p,bit) |= (hzaddress & 1) << PGET(cnt,bit);
    ++PGET(cnt,bit);
  }
  xyz[0] = p.x;
  xyz[1] = p.y;
  xyz[2] = p.z;
  xyz[3] = p.u;
  xyz[4] = p.v;
}

long long xyz_to_HZ(const char* bitmask, int maxh, PointND xyz)
{
  long long zaddress=0;
  int cnt   = 0;
  PointND zero;
  int temp_maxh = maxh;
  memset(&zero,0,sizeof(PointND));

  //VisusDebugAssert(VisusGetBitmaskBit(bitmask,0)=='V');
  for (cnt=0 ; memcmp(&xyz, &zero, sizeof(PointND)) ; cnt++, maxh--)
  {
    int bit= bitmask[maxh];
    zaddress |= ((long long)PGET(xyz,bit) & 1) << cnt;
    PGET(xyz,bit) >>= 1;
  }
  
  long long lastbitmask=((long long)1)<<temp_maxh;
  zaddress |= lastbitmask;
  while (!(1 & zaddress)) zaddress >>= 1;
    zaddress >>= 1;

  return zaddress;
}

int VisusSplitFilename(const char* filename,char* dirname,char* basename)
{
  int i;
  int N=strlen(filename);

  if (!N)
    return 0;

  //find the last separator
  for (i=N-1;i>=0;i--)
  {
    if (filename[i]=='/' || filename[i]=='\\')
    {
      strncpy(dirname,filename,i);
      dirname[i]=0;
      strcpy(basename,filename+i+1);
      return 1;
    }
  }
  //assume is all filename (without directory name)
  dirname [0]=0;
  strcpy(basename,filename);
  return 1;
}


int generate_file_name(int blocks_per_file, char* filename_template, int file_number, char* filename, int maxlen) 
{
  long long address = 0;
  unsigned int segs[MAX_TEMPLATE_DEPTH] = {0};
  int seg_count = 0;
  char* pos;
  int ret;

  //printf("[generate_file_name]: %d %s %d :: %s\n", file_number, filename, maxlen, filename_template);
  // determine the first HZ address for the file in question 
  address = file_number * blocks_per_file;

  // walk backwards through the file name template to find places where we need to substitute strings
  for (pos = &filename_template[strlen(filename_template) - 1];
	  pos != filename_template;
	  pos--) 
  {
    // be careful not to lo0 past the end of the array 
    if (pos - filename_template > (strlen(filename_template) - 3))
      continue;

    if (pos[0] == '%' && pos[1] == '0' && pos[3] == 'x') 
    {
      // TODO: for now we have a hard coded max depth 
      if (seg_count >= MAX_TEMPLATE_DEPTH) 
      {
	fprintf(stderr, "Error: generate_filename() function can't handle this template yet: %s\n", filename_template);
	return 1;
      }

      // found an occurance of %0 in the template; check the next character to see how many bits to use here 

      switch (pos[2]) 
      {
	case '1':
	    segs[seg_count] += address & 0xf;
	    address = address >> 4;
	    break;
	case '2':
	    segs[seg_count] += address & 0xff;
	    address = address >> 8;
	    break;
	case '3':
	    segs[seg_count] += address & 0xfff;
	    address = address >> 12;
	    break;
	case '4':
	    segs[seg_count] += address & 0xffff;
	    address = address >> 16;
	    break;
	case '5':
	    segs[seg_count] += address & 0xfffff;
	    address = address >> 20;
	    break;
	default:
	    // TODO: generalize this to work for any value 
	    fprintf(stderr, "Error: generate_filename() function can't handle this template yet: %s\n", filename_template);
	    return 1;
      }
      seg_count++;
    }
  }
  switch (seg_count) 
  {
    case 0:
	ret = strlen(filename_template);
	if (ret < maxlen) {
	    strcpy(filename, filename_template);
	}
	break;
    case 1:
	ret = snprintf(filename, maxlen, filename_template, segs[0]);
	break;
    case 2:
	ret = snprintf(filename, maxlen, filename_template,
		segs[1], segs[0]);
	break;
    case 3:
	ret = snprintf(filename, maxlen, filename_template,
		segs[2], segs[1], segs[0]);
	break;
    case 4:
	ret = snprintf(filename, maxlen, filename_template,
		segs[3], segs[2], segs[1], segs[0]);
	break;
    case 5:
	ret = snprintf(filename, maxlen, filename_template,
		segs[4], segs[3], segs[2], segs[1], segs[0]);
	break;
    case 6:
	ret = snprintf(filename, maxlen, filename_template,
		segs[5], segs[4], segs[3], segs[2], segs[1], segs[0]);
	break;
    default:
	// TODO: generalize this 
	fprintf(stderr, "Error: generate_filename() function can't handle this template yet: %s\n", filename_template);
	return 1;
	break;
  }
  // make sure that the resulting string fit into the buffer ok 
  if (ret >= maxlen - 1) 
  {
    fprintf(stderr, "Error: filename too short in generate_filename()\n");
    return 1;
  }
  return 0;
}

/////////////////////////////////////////////////
PIDX_return_code generate_file_name_template(int maxh, int bits_per_block, char* filename, int current_time_step, char* filename_template)
{
  int N;
  char dirname[1024], basename[1024];
  int nbits_blocknumber;
  char* directory_path;
  char* data_set_path;
  
  directory_path = (char*) malloc(sizeof (char) * 1024);
  assert(directory_path);
  memset(directory_path, 0, sizeof (char) * 1024);

  data_set_path = (char*) malloc(sizeof (char) * 1024);
  assert(data_set_path);
  memset(data_set_path, 0, sizeof (char) * 1024);

  strncpy(directory_path, filename, strlen(filename) - 4);  
  sprintf(data_set_path, "%s/time%06d.idx", directory_path, current_time_step);
  
  free(directory_path);

  nbits_blocknumber = (maxh - bits_per_block - 1);
  VisusSplitFilename(data_set_path, dirname, basename);

  //remove suffix
  for (N = strlen(basename) - 1; N >= 0; N--) 
  {
    int ch = basename[N];
    basename[N] = 0;
    if (ch == '.') break;
  }

#if 0
  //if i put . as the first character, if I move files VisusOpen can do path remapping
  sprintf(pidx->filename_template, "./%s", basename);
#endif
  //pidx does not do path remapping 
  strcpy(filename_template, data_set_path);
  for (N = strlen(filename_template) - 1; N >= 0; N--) 
  {
    int ch = filename_template[N];
    filename_template[N] = 0;
    if (ch == '.') break;
  }

  //can happen if I have only only one block
  if (nbits_blocknumber == 0) 
    strcat(filename_template, "/%01x.bin");
   
  else 
  {
    //approximate to 4 bits
    if (nbits_blocknumber % 4) 
    {
      nbits_blocknumber += (4 - (nbits_blocknumber % 4));
      assert(!(nbits_blocknumber % 4));
    }
    if (nbits_blocknumber <= 8) 
      strcat(filename_template, "/%02x.bin"); //no directories, 256 files
    else if (nbits_blocknumber <= 12) 
      strcat(filename_template, "/%03x.bin"); //no directories, 4096 files
    else if (nbits_blocknumber <= 16) 
      strcat(filename_template, "/%04x.bin"); //no directories, 65536  files
    else 
    {
      while (nbits_blocknumber > 16) 
      {
	strcat(filename_template, "/%02x"); //256 subdirectories
	nbits_blocknumber -= 8;
      }
      strcat(filename_template, "/%04x.bin"); //max 65536  files
      nbits_blocknumber -= 16;
      assert(nbits_blocknumber <= 0);
    }
  }
  
  free(data_set_path);
  return PIDX_success;
}