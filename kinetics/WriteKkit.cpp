/**********************************************************************
** This program is part of 'MOOSE', the
** Messaging Object Oriented Simulation Environment.
**           Copyright (C) 2003-2012 Upinder S. Bhalla. and NCBS
** It is made available under the terms of the
** GNU Lesser General Public License version 2.1
** See the file COPYING.LIB for the full notice.
**********************************************************************/

#include <iomanip>
#include <fstream>
#include "../shell/Shell.h"
#include "../shell/Wildcard.h"

#include "header.h"
#include "../utility/utility.h"

#include "PoolBase.h"
#include "Pool.h"
#include "BufPool.h"
#include "ReacBase.h"
#include "EnzBase.h"
#include "lookupVolumeFromMesh.h"
#include <sstream>
#include <set>


void writeHeader( ofstream& fout, 
		double simdt, double plotdt, double maxtime, double defaultVol)
{
	time_t rawtime;
	time( &rawtime );

	fout << 
	"//genesis\n"
	"// kkit Version 11 flat dumpfile\n\n";
	fout << "// Saved on " << ctime( &rawtime ) << endl;
	fout << "include kkit {argv 1}\n";
	fout << "FASTDT = " << simdt << endl;
	fout << "SIMDT = " << simdt << endl;
	fout << "CONTROLDT = " << plotdt << endl;
	fout << "PLOTDT = " << plotdt << endl;
	fout << "MAXTIME = " << maxtime << endl;
	fout << "TRANSIENT_TIME = 2\n"
	"VARIABLE_DT_FLAG = 0\n";
	fout << "DEFAULT_VOL = " << defaultVol << endl;
	fout << "VERSION = 11.0\n"
	"setfield /file/modpath value ~/scripts/modules\n"
	"kparms\n\n";

	fout << 
	"initdump -version 3 -ignoreorphans 1\n"
	"simobjdump table input output alloced step_mode stepsize x y z\n"
	"simobjdump xtree path script namemode sizescale\n"
	"simobjdump xcoredraw xmin xmax ymin ymax\n"
	"simobjdump xtext editable\n"
	"simobjdump xgraph xmin xmax ymin ymax overlay\n"
	"simobjdump xplot pixflags script fg ysquish do_slope wy\n"
	"simobjdump group xtree_fg_req xtree_textfg_req plotfield expanded movealone \\\n"
  	"  link savename file version md5sum mod_save_flag x y z\n"
	"simobjdump geometry size dim shape outside xtree_fg_req xtree_textfg_req x y z\n"
	"simobjdump kpool DiffConst CoInit Co n nInit mwt nMin vol slave_enable \\\n"
  	"  geomname xtree_fg_req xtree_textfg_req x y z\n"
	"simobjdump kreac kf kb notes xtree_fg_req xtree_textfg_req x y z\n"
	"simobjdump kenz CoComplexInit CoComplex nComplexInit nComplex vol k1 k2 k3 \\\n"
  	"  keepconc usecomplex notes xtree_fg_req xtree_textfg_req link x y z\n"
	"simobjdump stim level1 width1 delay1 level2 width2 delay2 baselevel trig_time \\\n"
  	"  trig_mode notes xtree_fg_req xtree_textfg_req is_running x y z\n"
	"simobjdump xtab input output alloced step_mode stepsize notes editfunc \\\n"
  	"  xtree_fg_req xtree_textfg_req baselevel last_x last_y is_running x y z\n"
	"simobjdump kchan perm gmax Vm is_active use_nernst notes xtree_fg_req \\\n"
  	"  xtree_textfg_req x y z\n"
	"simobjdump transport input output alloced step_mode stepsize dt delay clock \\\n"
  	"  kf xtree_fg_req xtree_textfg_req x y z\n"
	"simobjdump proto x y z\n";
	//"simundump geometry /kinetics/geometry 0 1.6667e-19 3 sphere \"\" white black 0 0 0\n\n";
}


void writeReac( ofstream& fout, Id id,
				string colour, string textcolour,
			 	double x, double y )
{
	Id reacparentId = Field <ObjId>  :: get(id,"parent");
	string reacPar  = Field <string> :: get(reacparentId,"name");

	string reacname = Field<string> :: get(id, "name");
	//size_t pos = path.find( "/kinetics" );
	//path = path.substr( pos );
	double kf = Field< double >::get( id, "kf" );
	double kb = Field< double >::get( id, "kb" );

	fout << "simundump kreac /" << reacPar << "/" << reacname << " 0 " << 
			kf << " " << kb << " \"\" " << 
			colour << " " << textcolour << " " << x << " " << y << " 0\n";
}

unsigned int getSlaveEnable( Id id )
{
	static const Finfo* setNinitFinfo = 
			PoolBase::initCinfo()->findFinfo( "set_nInit" );
	static const Finfo* setConcInitFinfo = 
			PoolBase::initCinfo()->findFinfo( "set_concInit" );
	unsigned int ret = 0;
	vector< Id > src;
	if ( id.element()->cinfo()->isA( "BufPool" ) ) {
		if ( id.element()->getNeighbors( src, setConcInitFinfo ) > 0 ) {
				ret = 2;
		} else if ( id.element()->getNeighbors( src, setNinitFinfo ) > 0 ){
				ret = 4;
		}
	} else {
		return 0;
	}
	if ( ret == 0 )
			return 4; // Just simple buffered molecule
	if ( src[0].element()->cinfo()->isA( "StimulusTable" ) )
			return ret; // Following a table, this is fine.
	
	// Fallback: I have no idea what sent it the input, assume it is legit.
	return ret;
}

void writePool( ofstream& fout, Id id,
				string colour, string textcolour,
			 	double x, double y )
{
	Id poolparentId = Field <ObjId>  :: get(id,"parent");
	string poolPar  = Field <string> :: get(poolparentId,"name");

	string poolname = Field<string> :: get(id, "name");
	double diffConst = Field< double >::get( id, "diffConst" );
	double concInit = Field< double >::get( id, "concInit" );
	double conc = Field< double >::get( id, "conc" );
	double nInit = Field< double >::get( id, "nInit" );
	double n = Field< double >::get( id, "n" );
	double volume = Field< double >::get( id, "volume" );
	//TODO: check with Upi what is this slave_enable
	//unsigned int slave_enable = getSlaveEnable( id );
	unsigned int slave_enable = 0;
	fout << "simundump kpool /" << poolPar << "/"<< poolname << " 0 " <<
			diffConst << " " <<
			concInit << " " << 
			conc << " " <<
			n << " " <<
			nInit << " " <<
			0 << " " << 0 << " " << // mwt, nMin
			volume * NA * 1e-3  << " " << // volscale
			slave_enable << " " << //GENESIS FIELD HERE.
			poolPar << "/geometry " << 
			colour << " " << textcolour << " " << x << " " << y << " 0\n";
}

void writePlot( ofstream& fout, Id id,
				string colour, string textcolour,
			 	double x, double y )
{	
	string path = id.path();
	size_t pos = path.find( "/graphs" );
	if ( pos == string::npos ) 
		pos = path.find( "/moregraphs" );
		if ( pos == string::npos ) 
			return;
	path = path.substr( pos );
	fout << "simundump xplot " << path << " 3 524288 \\\n" << 
	"\"delete_plot.w <s> <d>; edit_plot.D <w>\" " << textcolour << " 0 0 1\n";
}

void writeGui( ofstream& fout )
{
	fout << "simundump xgraph /graphs/conc1 0 0 99 0.001 0.999 0\n"
	"simundump xgraph /graphs/conc2 0 0 100 0 1 0\n"
	"simundump xgraph /moregraphs/conc3 0 0 100 0 1 0\n"
	"simundump xgraph /moregraphs/conc4 0 0 100 0 1 0\n"
	"simundump xcoredraw /edit/draw 0 -6 4 -2 6\n"
	"simundump xtree /edit/draw/tree 0 \\\n"
	"  /kinetics/#[],/kinetics/#[]/#[],/kinetics/#[]/#[]/#[][TYPE!=proto],/kinetics/#[]/#[]/#[][TYPE!=linkinfo]/##[] \"edit_elm.D <v>; drag_from_edit.w <d> <S> <x> <y> <z>\" auto 0.6\n"
	"simundump xtext /file/notes 0 1\n";
}

void writeFooter( ofstream& fout )
{
	fout << "\nenddump\n";
	fout << "complete_loading\n";
}
/*
void getInfoFields( Id id, string& bg, string& fg, 
				double& x, double& y, double side, double dx )
{
	Id info = findInfo( id );
	if ( info != Id() ) {
		bg = Field< string >::get( info, "color" );
		fg = Field< string >::get( info, "textColor" );
		x = Field< double >::get( info, "x" );
		y = Field< double >::get( info, "y" );
	} else {
		bg = "cyan";
		fg = "black";
		x += dx;
		if ( x > side ) {
				x = 0;
				y += dx;
		}
	}
}
*/
string trimPath(Id id)
{	
	Id parentId = Field <ObjId>  :: get(id,"parent");
	string poolPar  = Field <string> :: get(parentId,"name");
	string path = Field <string> :: get(id,"path");
	string poolname = Field<string> :: get(id, "name");
	size_t pos = path.find("/"+poolPar);
	return path.substr(pos);
}

void storeReacMsgs( Id reac, vector< string >& msgs )
{	
	Id reacparentId = Field <ObjId>  :: get(reac,"parent");
	string reacPar  = Field <string> :: get(reacparentId,"name");
	string reacName = Field<string> :: get(reac,"name");

	vector < Id > srct = LookupField <string,vector < Id> >::get(reac, "neighbors","sub");
	for (vector <Id> :: iterator rsub = srct.begin();rsub != srct.end();rsub++)
	{	
		string s = "addmsg " + trimPath(*rsub) + " " + trimPath(reac) + " SUBSTRATE n";
		msgs.push_back( s );
		s = "addmsg " + trimPath(reac) + " " + trimPath( *rsub ) + 	" REAC A B";
		msgs.push_back( s );
	}
	vector < Id > prct = LookupField <string,vector < Id> >::get(reac, "neighbors","prd");
	for (vector <Id> :: iterator rprd = prct.begin();rprd != prct.end();rprd++)
	{
		string s = "addmsg " + trimPath( *rprd ) + " " + trimPath(reac) + " PRODUCT n";
		msgs.push_back( s );
		s = "addmsg " + trimPath(reac) + " " + trimPath( *rprd ) + " REAC B A";
		msgs.push_back( s );
	}
}

void storePlotMsgs( Id tab, vector< string >& msgs, Id pool, string bg)
{
	string tabPath = tab.path();
	string poolPath = Field <string> :: get(pool,"path");
	string poolName = Field <string> :: get(pool,"name");

	size_t pos = tabPath.find( "/graphs" );
	if ( pos == string::npos ) 
		pos = tabPath.find( "/moregraphs" );
		assert( pos != string::npos );
	tabPath = tabPath.substr( pos );
	string s = "addmsg " + trimPath( poolPath ) + " " + tabPath + 
	 			" PLOT Co *" + poolName + " *" + bg;
	msgs.push_back( s );
}

/**
 * A bunch of heuristics to find good SimTimes to use for kkit. 
 * Returns runTime.
 */
double estimateSimTimes( double& simDt, double& plotDt )
{
		double runTime = Field< double >::get( Id( 1 ), "runTime" );
		if ( runTime <= 0 )
				runTime = 100.0;
		vector< double > dts = 
				Field< vector< double> >::get( Id( 1 ), "dts" );
		simDt = dts[16];
		plotDt = dts[18];
		if ( plotDt <= 0 )
				plotDt = runTime / 200.0;
		if ( simDt == 0 )
				simDt = 0.01;
		if ( simDt > plotDt )
				simDt = plotDt / 100;

		return runTime;
}

/// Returns an estimate of the default volume used in the model.
double estimateDefaultVol( Id model )
{
		vector< Id > children = 
				Field< vector< Id > >::get( model, "children" );
		vector< double > vols;
		double maxVol = 0;
		for ( vector< Id >::iterator i = children.begin(); 
						i != children.end(); ++i ) {
				if ( i->element()->cinfo()->isA( "ChemCompt" ) ) {
						double v = Field< double >::get( *i, "volume" );
						if ( i->element()->getName() == "kinetics" )
								return v;
						vols.push_back( v );
						if ( maxVol < v ) 
								maxVol = v;
				}
		}
		if ( maxVol > 0 )
				return maxVol;
		return 1.0e-15;
}
void writeMsgs( ofstream& fout, const vector< string >& msgs )
{
	for ( vector< string >::const_iterator i = msgs.begin();
					i != msgs.end(); ++i )
			fout << *i << endl;
}

void writeKkit( Id model, const string& fname )
{		ofstream fout( fname.c_str(), ios::out );
		vector< ObjId > chemCompt;
		vector< string > msgs;
		double simDt;
		double plotDt;
		double runTime = estimateSimTimes( simDt, plotDt );
		double defaultVol = estimateDefaultVol( model );
		writeHeader( fout, simDt, plotDt, runTime, defaultVol );
		string bg = "cyan";
		string fg = "black";
		double x = 0;
		double y = 0;
		unsigned int num = wildcardFind( model.path() + "/##[ISA=ChemCompt]", chemCompt );
		if ( num == 0 ) {
			cout << "Warning: writeKkit:: No model found on " << model << 
					endl;
			return;
		}
		for ( vector< ObjId >::iterator itr = chemCompt.begin(); itr != chemCompt.end();itr++)
		{
			vector < unsigned int>dims;
      		unsigned int dims_size;
      		dims_size = 1;
      		unsigned index = 0;
      		string comptPath = Field<string>::get(*itr,"path");
      		string comptname = Field<string>::get(*itr,"name");
      		double size = Field<double>::get(ObjId(*itr,index),"Volume");
	  		unsigned int ndim = Field<unsigned int>::get(ObjId(*itr,index),"NumDimensions");
      		ostringstream geometry;
		   	geometry << "simundump geometry /" << comptname <<  "/geometry 0 " << size << " " << ndim << " sphere \"\" white black 0 0 0\n";
			fout << geometry.str() << endl;	
			
			/*  Species */
			vector< ObjId > Compt_spe;
		  	wildcardFind(comptPath+"/##[ISA=PoolBase]",Compt_spe);
		  	int species_size = 1;
		  	string objname;
		  	for (vector <ObjId> :: iterator itrp = Compt_spe.begin();itrp != Compt_spe.end();itrp++)
		    { 	string path = Field <string> :: get (*itrp,"path");
				Id poolparpath = Field <ObjId>  :: get(*itrp,"parent");
				string poolParCN = Field <string> :: get(poolparpath,"className");
				if (poolParCN != "ZombieEnz")
				{	Id annotaId( path+"/info");
			      	string noteClass = Field<string> :: get(annotaId,"className");
			      	string notes;
			      	double x = Field <double> :: get(annotaId,"x");
			      	double y = Field <double> :: get(annotaId,"y");
			      	string fg = Field <string> :: get(annotaId,"textColor");
			      	string bg = Field <string> :: get(annotaId,"color");
			      	writePool(fout, *itrp,bg,fg,x,y);
			    }
		  	} //species is closed
	  		
	  		/* Reaction */
			vector< ObjId > Compt_Reac;
			wildcardFind(comptPath+"/##[ISA=ReacBase]",Compt_Reac);
			for (vector <ObjId> :: iterator itrR= Compt_Reac.begin();itrR != Compt_Reac.end();itrR++)
			{ 	string path = Field<string> :: get(*itrR,"path");
			  	Id annotaId( path+"/info");
			    string noteClass = Field<string> :: get(annotaId,"className");
			    string notes;
			    double x = Field <double> :: get(annotaId,"x");
			    double y = Field <double> :: get(annotaId,"y");
			    string fg = Field <string> :: get(annotaId,"textColor");
			    string bg = Field <string> :: get(annotaId,"color");
			    writeReac( fout, *itrR, bg, fg, x, y );
				storeReacMsgs( *itrR, msgs );
			}// reaction
		} // Compartment
	writeGui ( fout);
	/* Table */
	vector< ObjId > table;
	wildcardFind(model.path()+"/##[ISA=Table2]",table);
	for (vector <ObjId> :: iterator itrT= table.begin();itrT != table.end();itrT++)
	{ 	
		string tabPath = Field <string> :: get(*itrT,"path");
		vector < Id > tabSrc = LookupField <string,vector < Id> >::get(*itrT, "neighbors","requestOut");
		for (vector <Id> :: iterator tabItem= tabSrc.begin();tabItem != tabSrc.end();tabItem++)
		{ 	string path = Field <string> :: get(*tabItem,"path");
			Id annotaId(path+"/info");
			double x = Field <double> :: get(annotaId,"x");
	      	double y = Field <double> :: get(annotaId,"y");
    	  	string bg = Field <string> :: get(annotaId,"textColor");
      		string fg = Field <string> :: get(annotaId,"color");
			writePlot( fout, *itrT, bg, fg, x, y );
			storePlotMsgs( *itrT, msgs,*tabItem,fg );

		}
	}// table
	writeMsgs( fout, msgs );
	writeFooter( fout );
}
