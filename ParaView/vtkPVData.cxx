/*=========================================================================

  Program:   Visualization Toolkit
  Module:    vtkPVData.cxx
  Language:  C++
  Date:      $Date$
  Version:   $Revision$

Copyright (c) 1998-2000 Kitware Inc. 469 Clifton Corporate Parkway,
Clifton Park, NY, 12065, USA.

All rights reserved. No part of this software may be reproduced, distributed,
or modified, in any form or by any means, without permission in writing from
Kitware Inc.

IN NO EVENT SHALL THE AUTHORS OR DISTRIBUTORS BE LIABLE TO ANY PARTY FOR
DIRECT, INDIRECT, SPECIAL, INCIDENTAL, OR CONSEQUENTIAL DAMAGES ARISING OUT
OF THE USE OF THIS SOFTWARE, ITS DOCUMENTATION, OR ANY DERIVATIVES THEREOF,
EVEN IF THE AUTHORS HAVE BEEN ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

THE AUTHORS AND DISTRIBUTORS SPECIFICALLY DISCLAIM ANY WARRANTIES, INCLUDING,
BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY, FITNESS FOR A
PARTICULAR PURPOSE, AND NON-INFRINGEMENT.  THIS SOFTWARE IS PROVIDED ON AN
"AS IS" BASIS, AND THE AUTHORS AND DISTRIBUTORS HAVE NO OBLIGATION TO PROVIDE
MAINTENANCE, SUPPORT, UPDATES, ENHANCEMENTS, OR MODIFICATIONS.

=========================================================================*/
#include "vtkActor.h"
#include "vtkDataSetMapper.h"
#include "vtkPVData.h"
#include "vtkPVPolyData.h" 
#include "vtkPVSource.h"
#include "vtkKWView.h"
#include "vtkPVWindow.h"
#include "vtkKWApplication.h"
#include "vtkPVContourFilter.h"
#include "vtkPVAssignment.h"
#include "vtkPVApplication.h"
#include "vtkPVActorComposite.h"
#include "vtkPVMenuButton.h"

int vtkPVDataCommand(ClientData cd, Tcl_Interp *interp,
		     int argc, char *argv[]);


//----------------------------------------------------------------------------
vtkPVData::vtkPVData()
{
  this->CommandFunction = vtkPVDataCommand;

  this->Data = NULL;
  this->PVSource = NULL;
  
  this->FiltersMenuButton = vtkPVMenuButton::New();
  
  this->Mapper = vtkDataSetMapper::New();
  this->Actor = vtkActor::New();
  this->Assignment = NULL;
  
  this->ActorComposite = vtkPVActorComposite::New();
}

//----------------------------------------------------------------------------
vtkPVData::~vtkPVData()
{
  this->SetAssignment(NULL);
  this->SetData(NULL);
  this->SetPVSource(NULL);

  this->FiltersMenuButton->Delete();
  this->FiltersMenuButton = NULL;
  
  this->Mapper->Delete();
  this->Mapper = NULL;
  
  this->Actor->Delete();
  this->Actor = NULL;
  
  this->ActorComposite->Delete();
  this->ActorComposite = NULL;
}

//----------------------------------------------------------------------------
vtkPVData* vtkPVData::New()
{
  return new vtkPVData();
}

//----------------------------------------------------------------------------
void vtkPVData::Clone(vtkPVApplication *pvApp)
{
  if (this->Application)
    {
    vtkErrorMacro("Application has already been set.");
    }
  this->SetApplication(pvApp);

  // Clone this object on every other process.
  pvApp->BroadcastScript("%s %s", this->GetClassName(), this->GetTclName());
}


//----------------------------------------------------------------------------
int vtkPVData::Create(char *args)
{
  if (this->Application == NULL)
    {
    vtkErrorMacro("Object has not been cloned yet.");
    return 0;
    }
  
  // create the top level
  this->Script("frame %s %s", this->GetWidgetName(), args);

  this->Update();
  
  this->FiltersMenuButton->SetParent(this);
  this->FiltersMenuButton->Create(this->Application, "");
  this->FiltersMenuButton->SetButtonText("Filters");
  this->FiltersMenuButton->AddCommand("vtkContourFilter", this,
				      "Contour");
  if (this->Data->GetPointData()->GetScalars() == NULL)
    {
    this->Script("%s entryconfigure 3 -state disabled",
		 this->FiltersMenuButton->GetMenu()->GetWidgetName());
    }
  else
    {
    this->Script("%s entryconfigure 3 -state normal",
		 this->FiltersMenuButton->GetMenu()->GetWidgetName());
    }
  
  this->Script("pack %s", this->FiltersMenuButton->GetWidgetName());

  return 1;
}

//----------------------------------------------------------------------------
void vtkPVData::Contour()
{
  vtkPVApplication *pvApp = (vtkPVApplication *)this->Application;
  vtkPVContourFilter *contour;
  float *range;
  
  contour = vtkPVContourFilter::New();
  contour->Clone(pvApp);
  
  contour->SetInput(this);
  
  range = this->Data->GetScalarRange();
  contour->SetValue(0, (range[1]-range[0])/2.0);
      
  contour->SetName("contour");

  vtkPVWindow *window = vtkPVWindow::SafeDownCast(
    this->GetPVSource()->GetView()->GetParentWindow());
  this->GetPVSource()->GetView()->AddComposite(contour);
//  this->GetPVSource()->VisibilityOff();
  
  window->SetCurrentSource(contour);
  window->GetSourceList()->Update();
  
//  this->GetPVSource()->GetView()->Render();
  
  contour->Delete();
//  pvd->Delete();
}

//----------------------------------------------------------------------------
vtkProp* vtkPVData::GetProp()
{
  return this->Actor;
}

vtkPVActorComposite* vtkPVData::GetActorComposite()
{
  return this->ActorComposite;
}

//----------------------------------------------------------------------------
vtkPVApplication* vtkPVData::GetPVApplication()
{
  if (this->Application == NULL)
    {
    return NULL;
    }
  
  if (this->Application->IsA("vtkPVApplication"))
    {  
    return (vtkPVApplication*)(this->Application);
    }
  else
    {
    vtkErrorMacro("Bad typecast");
    return NULL;
    } 
}

//----------------------------------------------------------------------------
// MAYBE WE SHOULD NOT REFERENCE COUNT HERE BECAUSE NO ONE BUT THE 
// SOURCE WIDGET WILL REFERENCE THE DATA WIDGET.
void vtkPVData::SetPVSource(vtkPVSource *source)
{
  if (this->PVSource == source)
    {
    return;
    }
  this->Modified();

  if (this->PVSource)
    {
    vtkPVSource *tmp = this->PVSource;
    this->PVSource = NULL;
    tmp->UnRegister(this);
    }
  if (source)
    {
    this->PVSource = source;
    source->Register(this);
    }
}


//----------------------------------------------------------------------------
void vtkPVData::SetAssignment(vtkPVAssignment *a)
{
  if (this->Assignment == a)
    {
    return;
    }

  vtkPVApplication *pvApp = this->GetPVApplication();
  if (pvApp && pvApp->GetController()->GetLocalProcessId() == 0)
    {
    pvApp->BroadcastScript("%s SetAssignment %s", this->GetTclName(), 
			   a->GetTclName());
    }
  
  this->ActorComposite->SetAssignment(a);
  
  if (this->Assignment)
    {
    this->Assignment->UnRegister(NULL);
    this->Assignment = NULL;
    }

  if (a)
    {
    this->Assignment = a;
    a->Register(this);
    }
}

  
//----------------------------------------------------------------------------
void vtkPVData::Update()
{
  if (this->Data == NULL)
    {
    vtkErrorMacro("No data object to update.");
    }

  if (this->Assignment)
    {
    this->Data->SetUpdateExtent(this->Assignment->GetPiece(),
                                this->Assignment->GetNumberOfPieces());
    }

  this->Data->Update();
}






