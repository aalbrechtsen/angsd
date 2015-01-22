#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <zlib.h>
#include <ctype.h>
#include <vector>
#include <map>
#include <cassert>
#include <ctype.h>
#include "../fet.c"
#define LENS 4096

typedef struct{
  double p;
  int n;
  double *dens;
  double *cum;
}rbinom;


int minDist = 10;
typedef struct{
  char allele1;
  char allele2;
  double freq;
}hapSite;

void print(FILE *fp,int p,hapSite &h){
  fprintf(fp,"hapmap\t%d\t%d\t%d\t%f\n",p,h.allele1,h.allele2,h.freq);
}


double ldbinom(int k, int n,double p){
  return lbinom(n,k)+k*log(p)+(n-k)*log(1-p);
}

//    l<-  dbinom(error,d,(1-x)*eps+x*freq)
double likeOld(double x,int len,int *seqDepth,int *nonMajor,double *freq,double eps){
  double t = 0;
  for(int i=0;i<len;i++)
    t += ldbinom(nonMajor[i],seqDepth[i],(1-x)*eps+x*freq[i]);
  return t;
}

//    l<-  dbinom(error,d,x*freq*(1-4*eps/3)+eps)
double likeNew(double x,int len,int *seqDepth,int *nonMajor,double *freq,double eps){
  double t = 0;
  for(int i=0;i<len;i++)
    t += ldbinom(nonMajor[i],seqDepth[i],x*freq[i]*(1-4*eps/3.0)+eps);
  return t;
}


double likeOldMom(int len,int *seqDepth,int *nonMajor,double *freq,double eps){
  //  return(mean(error/d-eps)/(mean(freq)-eps))
  double top = 0;
  double bot = 0;
  
  for(int i=0;i<len;i++){
    top += (1.0*nonMajor[i])/(1.0*seqDepth[i]);
    bot += freq[i];
  }
  top = top/(1.0*len)-eps;
  bot = bot/(1.0*len)-eps;
  return top/bot; 
}


double likeNewMom(int len,int *seqDepth,int *nonMajor,double *freq,double eps){
  //  return(mean(error/d-eps)/(mean(freq)-eps))
  double top = 0;
  double bot = 0;
  
  for(int i=0;i<len;i++){
    top += (1.0*nonMajor[i])/(1.0*seqDepth[i]);
    bot += freq[i];
  }
  top = top/(1.0*len)-eps;
  bot = bot/(1.0*len)*(1-4.0*eps/3.0);
  return top/bot; 
}


typedef std::map<int,hapSite> aMap;

const char *hapfile=NULL,*mapfile=NULL,*icounts=NULL;
typedef unsigned char uchar;

typedef std::vector<int> iv;
typedef std::vector<int*> iv2;

typedef struct{
  iv pos;
  iv dist;//0=snpsite;-1=one pos left of snp,+1=one pos right of snp
  std::vector<int *> cn;//four long , counts of C,C,G,T
  aMap myMap;
}dat;



rbinom *init_rbinom(double p,int n){
  //fprintf(stderr,"initializatin rbinom p:%f n:%d\n",p,n);
  rbinom *rb=new rbinom;
  rb->p=p;
  rb->n=n;
  rb->cum = new double[n+1];
  rb->dens= new double[n+1];

  double ts=0;
  for(int k=0;k<=n;k++){
    rb->dens[k] = lbinom(n,k)+k*log(p)+(n-k)*log(1-p);
    //    fprintf(stderr,"%e\n",exp(rb->dens[k]));
    ts += exp(rb->dens[k]);
  }
  rb->cum[0] = exp(rb->dens[0])/ts;
  for(int k=1;k<=n;k++){
    rb->cum[k] = exp(rb->dens[k])/ts+rb->cum[k-1];

  }
  return rb;
}

int simrbinom(double p){
  if(drand48()<(1-p))
    return 0;
  else
    return 1;
}

int sample(rbinom *rb){
  //  fprintf(stderr,"rb n:%d p:%f\n",rb->n,rb->p);
  double r = drand48();
  int k=0;
  if(r<rb->cum[0])
    return k;

  while(k++<=rb->n)
    if(rb->cum[k-1]<r &&r<=rb->cum[k])
      break;
  return k;
 
}


void analysis(dat &d){
  int *rowSum = new int[d.cn.size()];
  int *rowMax = new int[d.cn.size()];
  int *rowMaxW = new int[d.cn.size()];
  int *error1 = new int[d.cn.size()];
  int *error2 = new int[d.cn.size()];
  size_t mat1[4]={0,0,0,0};
  size_t mat2[4]={0,0,0,0};
  size_t tab[2] = {0,0};
  for(int i=0;i<d.cn.size();i++){
    int s =d.cn[i][0];
    int max=s;
    int which=0;
    for(int j=1;j<4;j++){
      s += d.cn[i][j];
      if(d.cn[i][j]>max){
	max=d.cn[i][j];
	which=j;
      }
    }
    rowSum[i] = s;
    rowMax[i]=max;
    rowMaxW[i]=which;
    aMap::iterator it= d.myMap.find(d.pos[i]);
    if(it!=d.myMap.end()){
      // fprintf(stderr,"posi:%d wmax:%d all1:%d freq:%f\n",it->first,rowMaxW[i],it->second.allele1,it->second.freq);
      
      if(rowMaxW[i]==it->second.allele1)
	it->second.freq=1-it->second.freq;
      else
	it->second.freq=it->second.freq;
      // fprintf(stderr,"posi:%d wmax:%d all1:%d freq:%f\n",it->first,rowMaxW[i],it->second.allele1,it->second.freq);
      // exit(0);
    }

    error1[i] = rowSum[i]-rowMax[i];
    error2[i] = simrbinom((1.0*error1[i])/(1.0*rowSum[i]));
    //   fprintf(stdout,"rs\t%d %d %d %d %d %d\n",rowSum[i],rowMax[i],rowMaxW[i],error1[i],error2[i],d.dist[i]);
    if(error1[i]>0)
      tab[1]++;
    else
      tab[0]++;
    if(d.dist[i]==0){
      mat1[0] +=error1[i];
      mat1[1] +=rowSum[i]-error1[i];
      mat2[0] +=error2[i];
      mat2[1] +=1-error2[i];
      // fprintf(stdout,"rs %d %d %d %d %d %d %d %f %d %d\n",d.pos[i],rowSum[i],rowMax[i],rowMaxW[i],error1[i],error2[i],d.dist[i],it->second.freq,it->second.allele1,it->second.allele2);
    }else{
      mat1[2] +=error1[i];
      mat1[3] +=rowSum[i]-error1[i];
      mat2[2] += error2[i];
      mat2[3] += 1-error2[i];
    }
  }
  fprintf(stderr,"tab:%lu %lu\n",tab[0],tab[1]);
  fprintf(stderr,"mat: %lu %lu %lu %lu\n",mat1[0],mat1[1],mat1[2],mat1[3]);
  fprintf(stderr,"mat2: %lu %lu %lu %lu\n",mat2[0],mat2[1],mat2[2],mat2[3]);
  
  int n11, n12, n21, n22;
  double left, right, twotail, prob;
  
  n11=mat1[0];n12=mat1[2];n21=mat1[1];n22=mat1[3];
  prob = kt_fisher_exact(n11, n12, n21, n22, &left, &right, &twotail);
  fprintf(stdout,"Method\t n11 n12 n21 n22 prob left right twotail\n");
  fprintf(stdout,"%s\t%d\t%d\t%d\t%d\t%.6g\t%.6g\t%.6g\t%.6g\n", "method1", n11, n12, n21, n22,
				prob, left, right, twotail);

  n11=mat2[0];n12=mat2[2];n21=mat2[1];n22=mat2[3];
  prob = kt_fisher_exact(n11, n12, n21, n22, &left, &right, &twotail);
  fprintf(stdout,"%s\t%d\t%d\t%d\t%d\t%.6g\t%.6g\t%.6g\t%.6g\n", "method2", n11, n12, n21, n22,
				prob, left, right, twotail);

  //estimate how much contamination
  double c= mat1[2]/(1.0*(mat1[2]+mat1[3]));
  double err= mat1[0]/(1.0*(mat1[0]+mat1[1]));
  fprintf(stderr,"c:%f err:%f len:%lu lenpos:%lu\n",c,err,d.cn.size()/9,d.pos.size());

  int *err0 =new int[d.cn.size()/9];
  int *err1 =new int[d.cn.size()/9];
  int *d0 =new int[d.cn.size()/9];
  int *d1 =new int[d.cn.size()/9];
  double *freq =new double[d.cn.size()/9];

  for(int i=0;i<d.cn.size()/9;i++){
    int adj=0;
    int dep=0;
    for(int j=0;j<9;j++){
      
      if(d.dist[i*9+j]!=0){
	adj += error1[i*9+j];
	dep += rowSum[i*9+j];
      }else{
	err0[i] = error1[i*9+j];
	d0[i] = rowSum[i*9+j];
	freq[i] =d.myMap.find(d.pos[i*9+j])->second.freq;
      }
    }
    err1[i] =adj;
    d1[i] = dep;
#if 0
    if(it==d.myMap.end()){
      fprintf(stderr,"Problem finding:%d\n",d.pos[i]);
      exit(0);
    }
#endif
    //    fprintf(stdout,"cont\t%d\t%d\t%d\t%d\t%f\n",err0[i],err1[i],d0[i],d1[i],freq[i]);
    
  }

  double llh=likeOld(0.027,d.cn.size()/9,d0,err0,freq,c);
  double mom=likeOldMom(d.cn.size()/9,d0,err0,freq,c);
  fprintf(stderr,"llh:%f mom:%f\n",llh,mom);


  llh=likeNew(0.027,d.cn.size()/9,d0,err0,freq,c);
  mom=likeNewMom(d.cn.size()/9,d0,err0,freq,c);
  fprintf(stderr,"llh:%f mom:%f\n",llh,mom);
 
}

void print(int *ary,FILE *fp,size_t l,char *pre){
  fprintf(fp,"%s\t",pre);
  for(size_t i=0;i<l;i++)
    fprintf(fp,"%d\t",ary[i]);
  fprintf(fp,"\n");
}

#define NVAL -66
dat count(aMap &myMap,std::vector<int> &ipos,std::vector<int*> &cnt){
  int lastP = std::max((--myMap.end())->first,ipos[ipos.size()-1])+5;//<-add five so we dont step out

  char *hit = new char[lastP];
  memset(hit,NVAL,lastP);//-10 just indicate no value..
  
  for(aMap::iterator it=myMap.begin();it!=myMap.end();++it){
      for(int p=0;p<5;p++){
        hit[it->first+p] =hit[it->first-p] = p;
	hit[it->first-p] = -  hit[it->first-p] ;
      }
  }

  //now hit contains long stretches of NVAL and -4,...4.
  //now set NVAL to all the places where we don't have data
  char *aa= new char[lastP];
  memset(aa,NVAL,lastP);
  for(int i=0;i<ipos.size();i++)
    aa[ipos[i]] = 1; 
  //now loop over hit array and set to NVAL if no data
  for(int i=0;i<lastP;i++)
    if(aa[i]==NVAL)
      hit[i] = NVAL;

  //now loop over hitarray and make remove non -4,..4: segments
  int i=0;
  while(i<lastP){
    if(hit[i]!=-4)
      hit[i] = NVAL;
    else{
      int isOk=1;
      for(int j=0;j<9;j++)
	if(hit[i+j]!=j-4){
	  isOk=0;
	  break;
	}
      if(isOk==1)
	i+=9;
      else{
	hit[i]=NVAL;
      }
    }
    i++;
  }

  dat d;
  for(int i=0;i<ipos.size();i++)
    if(hit[ipos[i]]!=NVAL) { 
      d.pos.push_back(ipos[i]);
      d.dist.push_back(hit[ipos[i]]);
      
      d.cn.push_back(cnt[i]);//plugs in pointer to 4ints.
      
      if(hit[ipos[i]]==0){//this is a snpsite
	aMap::iterator it=myMap.find(ipos[i]);
	if(it==myMap.end()){
	  fprintf(stderr,"Problem finding snpsite:%d\n",ipos[i]);
	  exit(0);
	}
	aMap::iterator it2=d.myMap.find(ipos[i]);
	assert(it2==d.myMap.end());
	d.myMap[it->first] = it->second;

      }
    }
  fprintf(stderr,"nSNP sites: %lu, with flanking: %lu\n",d.myMap.size(),d.cn.size());
  return d;
}


char flip(char c){
  c = toupper(c);
  if(c=='A')
    return 'T';
  if(c=='T')
    return 'A';
  if(c=='G')
    return 'C';
  if(c=='C')
    return 'G';
  if(c==0)
    return 3;
  if(c==1)
    return 2;
  if(c==2)
    return 1;
  if(c==3)
    return 0;
  fprintf(stderr,"Problem interpreting char:%c\n",c);
  return 0;
}

gzFile getgz(const char *fname,const char *mode){
  gzFile gz=Z_NULL;
  gz = gzopen(fname,"rb");
  if(gz==Z_NULL){
    fprintf(stderr,"Problem opening file: %s\n",fname);
    exit(0);
  }
  return gz;
}

void printhapsite(hapSite &hs,FILE *fp,int &p){
  fprintf(fp,"p:%d al1:%c al2:%c freq:%f\n",p,hs.allele1,hs.allele2,hs.freq);
}

int charToNum(char c){
  if(c=='A'||c=='a')
    return 0;
  if(c=='C'||c=='c')
    return 1;
  if(c=='G'||c=='g')
    return 2;
  if(c=='T'||c=='t')
    return 3;
  // fprintf(stderr,"Problem with observed char: '%c'\n",c);
  return -1;
}


aMap readhap(const char *fname,int minDist=10){
  fprintf(stderr,"[%s] fname:%s minDist:%d\n",__FUNCTION__,fname,minDist);
  gzFile gz=getgz(fname,"rb");
  char buf[LENS];
  int viggo=3;
  aMap myMap;
  while(gzgets(gz,buf,LENS)){
    hapSite hs;
    int p = atoi(strtok(buf,"\t\n "));
    hs.allele1 = charToNum(strtok(NULL,"\t\n ")[0]);
    hs.freq = atof(strtok(NULL,"\t\n "));
    char strand= strtok(NULL,"\t\n ")[0];
    hs.allele2 = charToNum(strtok(NULL,"\t\n ")[0]);
    if(hs.allele1==-1||hs.allele2==-1)
      continue;
    if(strand=='-'){
      hs.allele1 = flip(hs.allele1);
      hs.allele2 = flip(hs.allele2);
    }
    
    if(myMap.count(p)>0){
      if(viggo>0){
	fprintf(stderr,"[%s] Duplicate positions found in file: %s, pos:%d\n",__FUNCTION__,fname,p);
	fprintf(stderr,"[%s] Will only use first entry\n",__FUNCTION__);
	fprintf(stderr,"[%s] This message is only printed 3 times\n",__FUNCTION__);
	viggo--;
      }
    }else{
      myMap[p]=hs;
    }
  }
  fprintf(stderr,"[%s] We have read: %zu sites from hapfile:%s\n",__FUNCTION__,myMap.size(),fname);
  fprintf(stderr,"[%s] will remove snp sites to close:\n",__FUNCTION__);


  int *vec = new int[myMap.size() -1];
  aMap::iterator it = myMap.begin();
  for(int i=0;i<myMap.size()-1;i++){
    aMap::iterator it2 = it;
    it2++;
    vec[i]=it2->first - it->first;
    it=it2;
  }
  it = myMap.begin();
  aMap newMap;
  for(int i=0;i<myMap.size()-1;i++){
    if(fabs(vec[i])>=minDist){
      newMap[it->first] = it->second;
      //     fprintf(stdout,"test\t%d\n",it->first);
    }
    it++;
  }
  newMap[it->first] = it->second;
  //..  exit(0);
  delete [] vec;
  fprintf(stderr,"[%s] We now have: %lu snpSites\n",__FUNCTION__,newMap.size());

#if 0
  for(aMap::iterator it=newMap.begin();it!=newMap.end();++it)
    print(stdout,it->first,it->second);
#endif
  

  return newMap;
}
 


void readicnts(const char *fname,std::vector<int> &ipos,std::vector<int*> &cnt,int minDepth,int maxDepth){
  fprintf(stderr,"[%s] fname:%s minDepth:%d maxDepth:%d\n",__FUNCTION__,fname,minDepth,maxDepth);
  gzFile gz=getgz(fname,"rb");

  int tmp[5];
  int totSite=0;
  while(gzread(gz,tmp,sizeof(int)*5)){
    totSite++;
    int *tmp1=new int[4];
    int d=0;
    for(int i=0;i<4;i++) {
      tmp1[i]=tmp[i+1];
      d += tmp1[i];
    }
   
    if(d>=minDepth&&d<=maxDepth){
      cnt.push_back(tmp1);
      // print(cnt[cnt.size()-1],stdout,4,"pre");
      //exit(0);
      ipos.push_back(tmp[0]-1);
    }
  }
  //  print(cnt[0],stderr,4,"dung");
  fprintf(stderr,"Has read:%d sites,  %zu sites (after depfilter) from ANGSD icnts file\n",totSite,ipos.size());
}

int main(int argc,char**argv){
#if 0
  rbinom *rb=init_rbinom(0.23,1);
  for(int i=0;i<1e7;i++)
    fprintf(stdout,"%d\n",sample(rb));
  return 0;
#endif
  int minDepth=2;
  int maxDepth=20;
  hapfile="../RES/hapMapCeuXlift.map.gz";
  icounts="../angsdput.icnts.gz";
  mapfile="../RES/chrX.unique.gz";
  aMap myMap = readhap(hapfile);
  std::vector<int> ipos;
  std::vector<int*> cnt;
  readicnts(icounts,ipos,cnt,minDepth,maxDepth);
  dat d=count(myMap,ipos,cnt);
  analysis(d);
  return 0;
}
