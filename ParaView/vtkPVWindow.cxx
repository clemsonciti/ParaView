/*=========================================================================

  Program:   Visualization Toolkit
  Module:    vtkPVWindow.cxx
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
#include "vtkPVApplication.h"
#include "vtkKWToolbar.h"
#include "vtkPVWindow.h"
#include "vtkOutlineFilter.h"
#include "vtkObjectFactory.h"
#include "vtkKWDialog.h"
#include "vtkKWNotebook.h"

#include "vtkInteractorStylePlaneSource.h"
#include "vtkInteractorStyleTrackballCamera.h"

#include "vtkMath.h"
#include "vtkSynchronizedTemplates3D.h"

#include "vtkPVAssignment.h"
#include "vtkPVComposite.h"
#include "vtkPVConeSource.h"
#include "vtkPVPolyData.h"
#include "vtkPVImageReader.h"
#include "vtkPVImage.h"
#include "vtkPVDataList.h"

//----------------------------------------------------------------------------
vtkPVWindow* vtkPVWindow::New()
{
  // First try to create the object from the vtkObjectFactory
  vtkObject* ret = vtkObjectFactory::CreateInstance("vtkPVWindow");
  if(ret)
    {
    return (vtkPVWindow*)ret;
    }
  // If the factory was unable to create the object, then create it here.
  return new vtkPVWindow;
}

int vtkPVWindowCommand(ClientData cd, Tcl_Interp *interp,
                             int argc, char *argv[]);

//----------------------------------------------------------------------------
vtkPVWindow::vtkPVWindow()
{  
  this->CommandFunction = vtkPVWindowCommand;
  this->RetrieveMenu = vtkKWMenu::New();
  this->CreateMenu = vtkKWMenu::New();
  this->Toolbar = vtkKWToolbar::New();
  this->ResetCameraButton = vtkKWWidget::New();
  this->PreviousCompositeButton = vtkKWWidget::New();
  this->NextCompositeButton = vtkKWWidget::New();
  this->CurrentDataComposite = NULL;
  this->CompositeList = vtkKWCompositeCollection::New();
  
  this->MenuSource = vtkKWMenu::New();
  this->MenuSource->SetParent(this->Menu);
    
  this->DataPropertiesFrame = vtkKWNotebook::New();
  this->DataPropertiesFrameCreated = 0;
  
  this->DataList = vtkPVDataList::New();
  this->DataList->SetCompositeCollection(this->CompositeList);
}

//----------------------------------------------------------------------------
vtkPVWindow::~vtkPVWindow()
{
  this->Toolbar->Delete();
  this->Toolbar = NULL;
  this->ResetCameraButton->Delete();
  this->ResetCameraButton = NULL;
  this->PreviousCompositeButton->Delete();
  this->PreviousCompositeButton = NULL;
  this->NextCompositeButton->Delete();
  this->NextCompositeButton = NULL;
  
  if (this->CurrentDataComposite != NULL)
    {
    this->CurrentDataComposite->Delete();
    this->CurrentDataComposite = NULL;
    }
  
  this->DataList->Delete();
  this->DataList = NULL;
  
  this->MenuSource->SetParent(NULL);
  this->MenuSource->Delete();
  this->MenuSource = NULL;
  
  this->DataPropertiesFrame->SetParent(NULL);
  this->DataPropertiesFrame->Delete();
  this->DataPropertiesFrame = NULL;
}

//----------------------------------------------------------------------------
void vtkPVWindow::Create(vtkKWApplication *app, char *args)
{
  // invoke super method first
  this->vtkKWWindow::Create(app,"");

  this->Script("wm geometry %s 900x700+0+0",
                      this->GetWidgetName());
  
  Tcl_Interp *interp = this->Application->GetMainInterp();

  // create the top level
  this->MenuFile->InsertCommand(0,"New Window", this, "NewWindow");

  // Create the menu for creating data sources.
  this->MenuSource->Create(app,"-tearoff 0");
  this->MenuFile->InsertCascade(1, "New Data", this->MenuSource, 0);
  this->MenuSource->AddCommand("Volume", this, "NewVolume");
  this->MenuSource->AddCommand("Cone", this, "NewCone");
  
  this->CreateMenu->SetParent(this->GetMenu());
  this->CreateMenu->Create(this->Application,"");
  this->Menu->InsertCascade(2,"Create",this->CreateMenu,0);

  this->RetrieveMenu->SetParent(this->GetMenu());
  this->RetrieveMenu->Create(this->Application,"");
  this->Menu->InsertCascade(3, "Retrieve", this->RetrieveMenu, 0);

  this->SetStatusText("Version 1.0 beta");
  this->CreateDefaultPropertiesParent();
  
  this->Script( "wm withdraw %s", this->GetWidgetName());

  // create the main view
  // we keep a handle to them as well
  this->MainView = vtkPVRenderView::New();
  this->MainView->Clone((vtkPVApplication*)app);
  this->MainView->SetParent(this->ViewFrame);
  this->MainView->Create(this->Application,"-width 200 -height 200");
  this->AddView(this->MainView);
  // force creation of the properties parent
  this->MainView->GetPropertiesParent();
  this->MainView->MakeSelected();
  this->MainView->ShowViewProperties();
  this->MainView->SetupBindings();
  this->MainView->Delete();
  this->Script( "pack %s -expand yes -fill both", 
                this->MainView->GetWidgetName());

  char cmd[1024];
  sprintf(cmd,"%s Open", this->GetTclName());
  
  this->Toolbar->SetParent(this->GetToolbarFrame());
  this->Toolbar->Create(app); 
  this->Script("pack %s -side left -pady 0 -fill none -expand no",
               this->Toolbar->GetWidgetName());
  
  this->ResetCameraButton->SetParent(this->Toolbar);
  this->ResetCameraButton->Create(app, "button", "-text ResetCamera");
  this->ResetCameraButton->SetCommand(this, "ResetCameraCallback");
  this->Script("pack %s -side left -pady 0 -fill none -expand no",
               this->ResetCameraButton->GetWidgetName());
  
  this->PreviousCompositeButton->SetParent(this->Toolbar);
  this->PreviousCompositeButton->Create(app, "button", "-text Previous");
  this->PreviousCompositeButton->SetCommand(this, "PreviousComposite");
  this->NextCompositeButton->SetParent(this->Toolbar);
  this->NextCompositeButton->Create(app, "button", "-text Next");
  this->NextCompositeButton->SetCommand(this, "NextComposite");
  this->Script("pack %s %s -side left -pady 0 -fill none -expand no",
	       this->PreviousCompositeButton->GetWidgetName(),
	       this->NextCompositeButton->GetWidgetName());
  
  this->DataList->SetParent(this->GetDataPropertiesParent());
  this->DataList->Create(app, "");
  this->Script("pack %s -side bottom -fill x -expand no",
	       this->DataList->GetWidgetName());
  
  this->Script( "wm deiconify %s", this->GetWidgetName());

  char *rbv =
    this->GetMenuProperties()->CreateRadioButtonVariable(
      this->GetMenuProperties(),"Radio");
  this->GetMenuProperties()->AddRadioButton(11,"Data", rbv, this, "ShowDataProperties");
  delete [] rbv;
  
  this->GetMenuProperties()->CheckRadioButton(this->GetMenuProperties(),
					      "Radio", 11);
  //not sure I should have to explicitly call this
  this->ShowDataProperties();
  
  // Setup an interactor style.
  vtkInteractorStyleTrackballCamera *style = vtkInteractorStyleTrackballCamera::New();
  this->MainView->SetInteractorStyle(style);
}

//----------------------------------------------------------------------------
void vtkPVWindow::ShowDataProperties()
{
  if (this->DataPropertiesFrameCreated == 0)
    {
    this->CreateDataPropertiesFrame();
    }

  this->Script("catch {eval pack forget [pack slaves %s]}",
	       this->GetPropertiesParent()->GetWidgetName());
  this->Script("pack %s",
	       this->DataPropertiesFrame->GetWidgetName());
}


//----------------------------------------------------------------------------
vtkKWWidget *vtkPVWindow::GetDataPropertiesParent()
{
  if (this->DataPropertiesFrameCreated == 0)
    {
    this->CreateDataPropertiesFrame();
    }
  
  return this->DataPropertiesFrame->GetFrame("Only");
}


//----------------------------------------------------------------------------
void vtkPVWindow::CreateDataPropertiesFrame()
{
  vtkKWApplication *app = this->Application;

  if (this->DataPropertiesFrameCreated)
    {
    return;
    }
  this->DataPropertiesFrameCreated = 1;
  
  this->DataPropertiesFrame->SetParent(this->GetPropertiesParent());
  this->DataPropertiesFrame->Create(this->Application,"-bd 0");
  this->DataPropertiesFrame->AddPage("Only");
}

void vtkPVWindow::SetCurrentDataComposite(vtkPVComposite *comp)
{
  if (comp->GetPropertiesParent() != this->GetDataPropertiesParent())
    {
    vtkErrorMacro("CurrentComposites must use our DataPropertiesParent.");
    return;
    }
  
  if (this->CurrentDataComposite)
    {
    this->CurrentDataComposite->Deselect(this->MainView);
    this->CurrentDataComposite->UnRegister(this);
    this->CurrentDataComposite = NULL;
    this->Script("catch {eval pack forget [pack slaves %s]}",
		 this->GetDataPropertiesParent()->GetWidgetName());
    }
  
  if (comp)
    {
    this->CurrentDataComposite = comp;
    comp->Select(this->MainView);
    comp->Register(this);
    if (this->CompositeList->IsItemPresent(comp) == 0)
      {
      this->CompositeList->AddItem(comp);
      }
    this->Script("pack %s %s -side top -anchor w -padx 2 -pady 2 -expand yes -fill x",
		 this->DataList->GetWidgetName(),
		 comp->GetProperties()->GetWidgetName());
    }
}

//----------------------------------------------------------------------------
// Setup a cone
// The first example that is not used anymore.
void vtkPVWindow::SetupCone()
{
  vtkPVApplication *pvApp = (vtkPVApplication *)this->Application;
  int id, num;
  
  // Lets show a cone as a test.
  num = pvApp->GetController()->GetNumberOfProcesses();
  for (id = 1; id < num; ++id)
    {
    pvApp->RemoteSimpleScript(id, "vtkConeSource ConeSource");
    pvApp->RemoteSimpleScript(id, "vtkPolyDataMapper ConeMapper");
    pvApp->RemoteSimpleScript(id, "ConeMapper SetInput [ConeSource GetOutput]");
    pvApp->RemoteSimpleScript(id, "vtkActor ConeActor");
    pvApp->RemoteSimpleScript(id, "ConeActor SetMapper ConeMapper");
    pvApp->RemoteSimpleScript(id, "[RenderSlave GetRenderer] AddActor ConeActor");
    // Shift to discount UI process
    pvApp->RemoteScript(id, "[ConeSource GetOutput] SetUpdateExtent %d %d", id-1, num-1);
    }
  
  // Setup an interactor style
  vtkInteractorStylePlaneSource *planeStyle = 
                      vtkInteractorStylePlaneSource::New();
  vtkInteractorStyle *style = vtkInteractorStyleTrackballCamera::New();

  //this->MainView->SetInteractorStyle(planeStyle);
  this->MainView->SetInteractorStyle(style);
  
  vtkActor *actor = planeStyle->GetPlaneActor();
  actor->GetProperty()->SetColor(0.0, 0.8, 0.0);
  
  //this->MainView->GetRenderer()->AddActor(actor);
  
  planeStyle->Delete();
  style->Delete();
  
  this->MainView->ResetCamera();
}

//----------------------------------------------------------------------------
void vtkPVWindow::NewCone()
{
  vtkPVApplication *pvApp = (vtkPVApplication *)this->Application;
  vtkPVComposite *comp;
  vtkPVConeSource *cone;
  vtkPVPolyData *pvd;
  vtkPVAssignment *a;
  
  // Maybe source and composite should be merged into one class.
  // Create the pipeline objects in all processes.
  comp = vtkPVComposite::New();
  comp->Clone(pvApp);
  cone = vtkPVConeSource::New();
  cone->Clone(pvApp);
  pvd = vtkPVPolyData::New();
  pvd->Clone(pvApp);
  a = vtkPVAssignment::New();
  a->Clone(pvApp);
  
  // Link the objects together (in all processes).
  cone->SetOutput(pvd);
  comp->SetSource(cone);
  comp->SetCompositeName("cone");
  cone->SetAssignment(a);
  
  // Create the properties (interface).
  comp->SetPropertiesParent(this->GetDataPropertiesParent());
  comp->CreateProperties("");

  // Add the new composite to the View (in all processes).
  this->MainView->AddComposite(comp);
  
  // We should probably use the View (instead of the window)
  // to manage the selected composite.
  comp->SetWindow(this);
  // Select this composite
  this->SetCurrentDataComposite(comp);

  // Clean up. (How about on the other processes?)
  comp->Delete();
  comp = NULL;
  this->DataList->Update();
  cone->Delete();
  cone = NULL;
  a->Delete();
  a = NULL;
  
  this->MainView->ResetCamera();
  this->MainView->Render();
}

//----------------------------------------------------------------------------
// Setup the pipeline
void vtkPVWindow::NewVolume()
{
  vtkPVApplication *pvApp = (vtkPVApplication *)this->Application;
  vtkPVComposite *comp;
  vtkPVImageReader *reader;
  vtkPVImage *image;
  vtkPVAssignment *a;

  
  comp = vtkPVComposite::New();
  comp->Clone(pvApp);
  reader = vtkPVImageReader::New();
  reader->Clone(pvApp);
  image = vtkPVImage::New();
  image->Clone(pvApp);
  a = vtkPVAssignment::New();
  a->Clone(pvApp);

  // Does not actually read.  Just sets the file name ...
  reader->ReadImage();
  
  reader->SetOutput(image);
  reader->SetAssignment(a);
  
  comp->SetSource(reader);
  comp->SetCompositeName("volume");
  comp->SetPropertiesParent(this->GetDataPropertiesParent());
  comp->CreateProperties("");
  this->MainView->AddComposite(comp);
  comp->SetWindow(this);
  this->SetCurrentDataComposite(comp);
  comp->Delete();
  this->DataList->Update();
  
  this->MainView->ResetCamera();
  this->MainView->Render();
}

//----------------------------------------------------------------------------
void vtkPVWindow::NewWindow()
{
  vtkPVWindow *nw = vtkPVWindow::New();
  nw->Create(this->Application,"");
  this->Application->AddWindow(nw);  
  nw->Delete();
}

//----------------------------------------------------------------------------
void vtkPVWindow::Save()
{
  //char *path;
  //
}

//----------------------------------------------------------------------------
// Description:
// Chaining method to serialize an object and its superclasses.
void vtkPVWindow::SerializeSelf(ostream& os, vtkIndent indent)
{
  // invoke superclass
  this->vtkKWWindow::SerializeSelf(os,indent);

  os << indent << "MainView ";
  this->MainView->Serialize(os,indent);
}


//----------------------------------------------------------------------------
void vtkPVWindow::SerializeToken(istream& is, const char token[1024])
{
  if (!strcmp(token,"MainView"))
    {
    this->MainView->Serialize(is);
    return;
    }

  vtkKWWindow::SerializeToken(is,token);
}

//----------------------------------------------------------------------------
vtkPVComposite* vtkPVWindow::GetCurrentDataComposite()
{
  return this->CurrentDataComposite;
}

//----------------------------------------------------------------------------
void vtkPVWindow::NextComposite()
{
  vtkPVComposite *composite = this->GetNextComposite();
  if (composite != NULL)
    {
    this->GetCurrentDataComposite()->GetProp()->VisibilityOff();
    this->SetCurrentDataComposite(composite);
    this->GetCurrentDataComposite()->GetProp()->VisibilityOn();
    }
  
  this->MainView->Render();
  this->DataList->Update();
}

//----------------------------------------------------------------------------
void vtkPVWindow::PreviousComposite()
{
  vtkPVComposite *composite = this->GetPreviousComposite();
  if (composite != NULL)
    {
    this->GetCurrentDataComposite()->GetProp()->VisibilityOff();
    this->SetCurrentDataComposite(composite);
    this->GetCurrentDataComposite()->GetProp()->VisibilityOn();
    }
  
  this->MainView->Render();
  this->DataList->Update();
}


//----------------------------------------------------------------------------
vtkPVComposite* vtkPVWindow::GetNextComposite()
{
  int pos = this->CompositeList->IsItemPresent(this->CurrentDataComposite);
  return (vtkPVComposite*)this->CompositeList->GetItemAsObject(pos);
}

//----------------------------------------------------------------------------
vtkPVComposite* vtkPVWindow::GetPreviousComposite()
{
  int pos = this->CompositeList->IsItemPresent(this->CurrentDataComposite);
  return (vtkPVComposite*)this->CompositeList->GetItemAsObject(pos-2);
}

//----------------------------------------------------------------------------
vtkKWCompositeCollection* vtkPVWindow::GetCompositeList()
{
  return this->CompositeList;
}

//----------------------------------------------------------------------------
void vtkPVWindow::ResetCameraCallback()
{
  
  this->MainView->ResetCamera();
  this->MainView->Render();
}



