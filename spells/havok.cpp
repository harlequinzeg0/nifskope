#include "../spellbook.h"

#include "../NvTriStrip/qtwrapper.h"

#include "blocks.h"

#include <QDebug>

// Brief description is deliberately not autolinked to class Spell
/*! \file havok.cpp
 * \brief Havok spells
 *
 * All classes here inherit from the Spell class.
 */

//Wz didn't provide enough code for this to work.  I don't see any reason QHull can't be put on SVN, but he seems to have changed
//the parameters of the example compute_convex_hull function, so I'll just leave it defined out until someone has a chance to dig
//into it or provide an alternative implementation without QHull.
#ifdef USE_QHULL

#include "../qhull.h"
#include <QDialog>
#include <QDoubleSpinBox>
#include <QLabel>
#include <QMap>
#include <QPushButton>
#include <QVBoxLayout>

//! Creates a convex hull
class spCreateCVS : public Spell
{
public:
	QString name() const { return Spell::tr("Create Convex Shape"); }
	QString page() const { return Spell::tr("Havok"); }
	
	bool isApplicable( const NifModel * nif, const QModelIndex & index )
	{
		if( !nif->inherits( index, "NiTriBasedGeom" ) )
			return false;
		
		QModelIndex iData = nif->getBlock( nif->getLink( index, "Data" ) );
		return iData.isValid();
	}
	
	QModelIndex cast( NifModel * nif, const QModelIndex & index )
	{
		QModelIndex iData = nif->getBlock( nif->getLink( index, "Data" ) );
		if( !iData.isValid() )
			return index;
		
		/* those will be filled with the CVS data */
		QVector<Vector4> convex_verts, convex_norms;
		
		/* get the verts of our mesh */
		QVector<Vector3> verts = nif->getArray<Vector3>( iData, "Vertices" );
		
		// to store results
		QVector<Vector4> hullVerts, hullNorms;

		// ask for precision
		QDialog dlg;
		QVBoxLayout * vbox = new QVBoxLayout;
		dlg.setLayout( vbox );
		
		vbox->addWidget( new QLabel( Spell::tr( "Enter the maximum roundoff error to use" ) ) );
		vbox->addWidget( new QLabel( Spell::tr( "Larger values will give a less precise but better performing hull" ) ) );
		
		QDoubleSpinBox * precSpin = new QDoubleSpinBox;
		precSpin->setRange( 0, 5 );
		precSpin->setDecimals( 3 );
		precSpin->setSingleStep( 0.01 );
		precSpin->setValue( 0.05 );
		vbox->addWidget( precSpin );

		QPushButton * ok = new QPushButton;
		ok->setText( Spell::tr( "Ok" ) );
		vbox->addWidget( ok );
		
		QObject::connect( ok, SIGNAL( clicked() ), &dlg, SLOT( accept() ) );
		
		// for the moment we don't care if the user can cancel
		dlg.exec();
		
		/* make a convex hull from it */
		compute_convex_hull( verts, hullVerts, hullNorms, (float) precSpin->value() );

		// consider moving the magic Havok scaling constant of 7.0 into qhull.cpp
		
		// sort and remove duplicate vertices
		QList<Vector4> sortedVerts;
		foreach( Vector4 vert, hullVerts )
		{
			vert /= 7.0;
			if( ! sortedVerts.contains( vert ) )
			{
				sortedVerts.append( vert );
				qWarning() << "Inserted " << vert;
			}
		}
		qSort( sortedVerts.begin(), sortedVerts.end(), Vector4::lexLessThan );
		QListIterator<Vector4> vertIter( sortedVerts );
		while( vertIter.hasNext() )
		{
			Vector4 sorted = vertIter.next();
			qWarning() << "Sorted value:" << sorted;
			convex_verts.append( sorted );
		}
		
		// sort and remove duplicate normals
		QList<Vector4> sortedNorms;
		foreach( Vector4 norm, hullNorms )
		{
			norm = Vector4( Vector3( norm ), norm[3] / 7.0 );
			if( ! sortedNorms.contains( norm ) )
			{
				sortedNorms.append( norm );
				qWarning() << "Inserted " << norm;
			}
		}
		qSort( sortedNorms.begin(), sortedNorms.end(), Vector4::lexLessThan );
		QListIterator<Vector4> normIter( sortedNorms );
		while( normIter.hasNext() )
		{
			Vector4 sorted = normIter.next();
			qWarning() << "Sorted value:" << sorted;
			convex_norms.append( sorted );
		}
		
		/* create the CVS block */
		QModelIndex iCVS = nif->insertNiBlock( "bhkConvexVerticesShape" );
		
		/* set CVS verts */
		nif->set<uint>( iCVS, "Num Vertices", convex_verts.count() );
		nif->updateArray( iCVS, "Vertices" );
		nif->setArray<Vector4>( iCVS, "Vertices", convex_verts );
		
		/* set CVS norms */
		nif->set<uint>( iCVS, "Num Normals", convex_norms.count() );
		nif->updateArray( iCVS, "Normals" );
		nif->setArray<Vector4>( iCVS, "Normals", convex_norms );

		// TODO: set radius, arrow detection [0, 0, -0, 0, 0, -0]

		QModelIndex iParent = nif->getBlock( nif->getParent( nif->getBlockNumber( index ) ) );

		qWarning() << "Parent is" << iParent;

		QModelIndex collisionLink = nif->getIndex( iParent, "Collision Object" );

		qWarning() << "Collision link" << collisionLink << "is a link:" << nif->isLink( collisionLink );

		QModelIndex collisionObject = nif->getBlock( nif->getLink( collisionLink ) );

		qWarning() << "Collision object is" << collisionObject;

		// create bhkCollisionObject
		if( ! collisionObject.isValid() )
		{
			qWarning() << "Collision object is empty, inserting one";
			collisionObject = nif->insertNiBlock( "bhkCollisionObject" );

			nif->setLink( collisionLink, nif->getBlockNumber( collisionObject ) );
			nif->setLink( collisionObject, "Target", nif->getBlockNumber( iParent ) );
		}

		QModelIndex rigidBodyLink = nif->getIndex( collisionObject, "Body" );

		qWarning() << "Rigid body link" << rigidBodyLink;

		QModelIndex rigidBody = nif->getBlock( nif->getLink( rigidBodyLink ) );

		// create bhkRigidBody
		if( ! rigidBody.isValid() )
		{
			qWarning() << "Rigid body is empty, inserting one";
			rigidBody = nif->insertNiBlock( "bhkRigidBody" );

			nif->setLink( rigidBodyLink, nif->getBlockNumber( rigidBody ) );
		}
		
		QModelIndex shapeLink = nif->getIndex( rigidBody, "Shape" );

		qWarning() << "Shape link" << shapeLink;

		QModelIndex shape = nif->getBlock( nif->getLink( shapeLink ) );

		// set link and delete old one
		nif->setLink( shapeLink, nif->getBlockNumber( iCVS ) );

		if( shape.isValid() )
		{
			spRemoveBranch BranchRemover;
			BranchRemover.castIfApplicable( nif, shape );
		}
		
		return index;
	}
};

REGISTER_SPELL( spCreateCVS );
#endif

//! Transforms Havok constraints
class spConstraintHelper : public Spell
{
public:
	QString name() const { return Spell::tr("A -> B"); }
	QString page() const { return Spell::tr("Havok"); }
	
	bool isApplicable( const NifModel * nif, const QModelIndex & index )
	{
		return nif && (
			nif->isNiBlock( nif->getBlock( index ), "bhkMalleableConstraint" )
			|| nif->isNiBlock( nif->getBlock( index ), "bhkRagdollConstraint" )
			|| nif->isNiBlock( nif->getBlock( index ), "bhkLimitedHingeConstraint" )
			|| nif->isNiBlock( nif->getBlock( index ), "bhkHingeConstraint" )
			|| nif->isNiBlock( nif->getBlock( index ), "bhkPrismaticConstraint" ) );
	}

	QModelIndex cast( NifModel * nif, const QModelIndex & index )
	{
		QModelIndex iConstraint = nif->getBlock( index );
		QString name = nif->itemName( iConstraint );
		if ( name == "bhkMalleableConstraint" )
		{
			if ( nif->getIndex( iConstraint, "Ragdoll" ).isValid() )
			{
				name = "bhkRagdollConstraint";
			}
			else if ( nif->getIndex( iConstraint, "Limited Hinge" ).isValid() )
			{
				name = "bhkLimitedHingeConstraint";
			}
			else if ( nif->getIndex( iConstraint, "Hinge" ).isValid() )
			{
				name = "bhkHingeConstraint";
			}
		}
		
		QModelIndex iBodyA = nif->getBlock( nif->getLink( nif->getIndex( iConstraint, "Entities" ).child( 0, 0 ) ), "bhkRigidBody" );
		QModelIndex iBodyB = nif->getBlock( nif->getLink( nif->getIndex( iConstraint, "Entities" ).child( 1, 0 ) ), "bhkRigidBody" );
		
		if ( ! iBodyA.isValid() || ! iBodyB.isValid() )
		{
			qWarning() << "coudn't find the bodies for this constraint";
			return index;
		}
		
		Transform transA = bodyTrans( nif, iBodyA );
		Transform transB = bodyTrans( nif, iBodyB );

		if ( name == "bhkLimitedHingeConstraint" )
		{
			iConstraint = nif->getIndex( iConstraint, "Limited Hinge" );
			if ( ! iConstraint.isValid() )
				return index;
		}

		if ( name == "bhkRagdollConstraint" )
		{
			iConstraint = nif->getIndex( iConstraint, "Ragdoll" );
			if ( ! iConstraint.isValid() )
				return index;
		}
		
		if ( name == "bhkHingeConstraint" )
		{
			iConstraint = nif->getIndex( iConstraint, "Hinge" );
			if ( ! iConstraint.isValid() )
				return index;
		}

		Vector3 pivot = Vector3( nif->get<Vector4>( iConstraint, "Pivot A" ) ) * 7.0;
		pivot = transA * pivot;
		pivot = transB.rotation.inverted() * ( pivot - transB.translation ) / transB.scale / 7.0;
		nif->set<Vector4>( iConstraint, "Pivot B", Vector4( pivot[0], pivot[1], pivot[2], 0 ) );
		
		if ( name == "bhkLimitedHingeConstraint" )
		{
			Vector3 axle = Vector3( nif->get<Vector4>( iConstraint, "Axle A" ) );
			axle = transA.rotation * axle;
			axle = transB.rotation.inverted() * axle;
			nif->set<Vector4>( iConstraint, "Axle B", Vector4( axle[0], axle[1], axle[2], 0 ) );
		
			axle = Vector3( nif->get<Vector4>( iConstraint, "Perp2 Axle In A2" ) );
			axle = transA.rotation * axle;
			axle = transB.rotation.inverted() * axle;
			nif->set<Vector4>( iConstraint, "Perp2 Axle In B2", Vector4( axle[0], axle[1], axle[2], 0 ) );
		}

		if ( name == "bhkRagdollConstraint" )
		{
			Vector3 axle = Vector3( nif->get<Vector4>( iConstraint, "Plane A" ) );
			axle = transA.rotation * axle;
			axle = transB.rotation.inverted() * axle;
			nif->set<Vector4>( iConstraint, "Plane B", Vector4( axle[0], axle[1], axle[2], 0 ) );

			axle = Vector3( nif->get<Vector4>( iConstraint, "Twist A" ) );
			axle = transA.rotation * axle;
			axle = transB.rotation.inverted() * axle;
			nif->set<Vector4>( iConstraint, "Twist B", Vector4( axle[0], axle[1], axle[2], 0 ) );
		}
		
		return index;
	}
	
	static Transform bodyTrans( const NifModel * nif, const QModelIndex & index )
	{
		Transform t;
		if ( nif->isNiBlock( index, "bhkRigidBodyT" ) )
		{
			t.translation = Vector3( nif->get<Vector4>( index, "Translation" ) * 7 );
			t.rotation.fromQuat( nif->get<Quat>( index, "Rotation" ) );
		}
		
		qint32 l = nif->getBlockNumber( index );
		
		while ( ( l = nif->getParent( l ) ) >= 0 )
		{
			QModelIndex iAV = nif->getBlock( l, "NiAVObject" );
			if ( iAV.isValid() )
				t = Transform( nif, iAV ) * t;
		}
		
		return t;
	}
};

REGISTER_SPELL( spConstraintHelper )

//! Calculates Havok spring lengths
class spStiffSpringHelper : public Spell
{
public:
	QString name() const { return Spell::tr( "Calculate Spring Length" ); }
	QString page() const { return Spell::tr( "Havok" ); }
	
	bool isApplicable( const NifModel * nif, const QModelIndex & idx )
	{
		return nif && nif->isNiBlock( nif->getBlock( idx ), "bhkStiffSpringConstraint" );
	}
	
	QModelIndex cast( NifModel * nif, const QModelIndex & idx )
	{
		QModelIndex iConstraint = nif->getBlock( idx );
		
		QModelIndex iBodyA = nif->getBlock( nif->getLink( nif->getIndex( iConstraint, "Entities" ).child( 0, 0 ) ), "bhkRigidBody" );
		QModelIndex iBodyB = nif->getBlock( nif->getLink( nif->getIndex( iConstraint, "Entities" ).child( 1, 0 ) ), "bhkRigidBody" );
		
		if ( ! iBodyA.isValid() || ! iBodyB.isValid() )
		{
			qWarning() << "coudn't find the bodies for this constraint";
			return idx;
		}
		
		Transform transA = spConstraintHelper::bodyTrans( nif, iBodyA );
		Transform transB = spConstraintHelper::bodyTrans( nif, iBodyB );
		
		Vector3 pivotA( nif->get<Vector4>( iConstraint, "Pivot A" ) * 7 );
		Vector3 pivotB( nif->get<Vector4>( iConstraint, "Pivot B" ) * 7 );
		
		float length = ( transA * pivotA - transB * pivotB ).length() / 7;
		
		nif->set<float>( iConstraint, "Length", length );
		
		return nif->getIndex( iConstraint, "Length" );
	}
};

REGISTER_SPELL( spStiffSpringHelper )

//! Packs Havok strips
class spPackHavokStrips : public Spell
{
public:
	QString name() const { return Spell::tr( "Pack Strips" ); }
	QString page() const { return Spell::tr( "Havok" ); }
	
	bool isApplicable( const NifModel * nif, const QModelIndex & idx )
	{
		return nif->isNiBlock( idx, "bhkNiTriStripsShape" );
	}
	
	QModelIndex cast( NifModel * nif, const QModelIndex & iBlock )
	{
		QPersistentModelIndex iShape( iBlock );
		
		QVector<Vector3> vertices;
		QVector<Triangle> triangles;
		QVector<Vector3> normals;
		
		foreach ( qint32 lData, nif->getLinkArray( iShape, "Strips Data" ) )
		{
			QModelIndex iData = nif->getBlock( lData, "NiTriStripsData" );
			
			if ( iData.isValid() )
			{
				QVector<Vector3> vrts = nif->getArray<Vector3>( iData, "Vertices" );
				QVector<Triangle> tris;
				QVector<Vector3> nrms;
				
				QModelIndex iPoints = nif->getIndex( iData, "Points" );
				for ( int x = 0; x < nif->rowCount( iPoints ); x++ )
				{
					tris += triangulate( nif->getArray<quint16>( iPoints.child( x, 0 ) ) );
				}
				
				QMutableVectorIterator<Triangle> it( tris );
				while ( it.hasNext() )
				{
					Triangle & tri = it.next();
					
					Vector3 a = vrts.value( tri[0] );
					Vector3 b = vrts.value( tri[1] );
					Vector3 c = vrts.value( tri[2] );
					
					nrms << Vector3::crossproduct( b - a, c - a ).normalize();
					
					tri[0] += vertices.count();
					tri[1] += vertices.count();
					tri[2] += vertices.count();
				}
				
				foreach ( Vector3 v, vrts )
					vertices += v / 7;
				triangles += tris;
				normals += nrms;
			}
		}
		
		if ( vertices.isEmpty() || triangles.isEmpty() )
		{
			qWarning() << Spell::tr( "no mesh data was found" );
			return iShape;
		}
		
		QPersistentModelIndex iPackedShape = nif->insertNiBlock( "bhkPackedNiTriStripsShape", nif->getBlockNumber( iShape ) );
		
		nif->set<int>( iPackedShape, "Num Sub Shapes", 1 );
		QModelIndex iSubShapes = nif->getIndex( iPackedShape, "Sub Shapes" );
		nif->updateArray( iSubShapes );
		nif->set<int>( iSubShapes.child( 0, 0 ), "Layer", 1 );
		nif->set<int>( iSubShapes.child( 0, 0 ), "Num Vertices", vertices.count() );
		nif->setArray<float>( iPackedShape, "Unknown Floats", QVector<float>() << 0.0f << 0.0f << 0.1f << 0.0f << 1.0f << 1.0f << 1.0f << 1.0f << 0.1f );
		nif->set<float>( iPackedShape, "Scale", 1.0f );
		nif->setArray<float>( iPackedShape, "Unknown Floats 2", QVector<float>() << 1.0f << 1.0f << 1.0f );
		
		QModelIndex iPackedData = nif->insertNiBlock( "hkPackedNiTriStripsData", nif->getBlockNumber( iPackedShape ) );
		nif->setLink( iPackedShape, "Data", nif->getBlockNumber( iPackedData ) );
		
		nif->set<int>( iPackedData, "Num Triangles", triangles.count() );
		QModelIndex iTriangles = nif->getIndex( iPackedData, "Triangles" );
		nif->updateArray( iTriangles );
		for ( int t = 0; t < triangles.size(); t++ )
		{
			nif->set<Triangle>( iTriangles.child( t, 0 ), "Triangle", triangles[ t ] );
			nif->set<Vector3>( iTriangles.child( t, 0 ), "Normal", normals.value( t ) );
		}
		
		nif->set<int>( iPackedData, "Num Vertices", vertices.count() );
		QModelIndex iVertices = nif->getIndex( iPackedData, "Vertices" );
		nif->updateArray( iVertices );
		nif->setArray<Vector3>( iVertices, vertices );
		
		QMap<qint32,qint32> lnkmap;
		lnkmap.insert( nif->getBlockNumber( iShape ), nif->getBlockNumber( iPackedShape ) );
		nif->mapLinks( lnkmap );
		
		// *** THIS SOMETIMES CRASHES NIFSKOPE        ***
		// *** UNCOMMENT WHEN BRANCH REMOVER IS FIXED ***
		// See issue #2508255
		spRemoveBranch BranchRemover;
		BranchRemover.castIfApplicable( nif, iShape );
		
		return iPackedShape;
	}
};

REGISTER_SPELL( spPackHavokStrips )

