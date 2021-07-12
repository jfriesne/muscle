object DelphiShareClient: TDelphiShareClient
  Left = 278
  Top = 252
  Width = 526
  Height = 460
  Caption = 'DelphiChat'
  Color = clBtnFace
  Font.Charset = DEFAULT_CHARSET
  Font.Color = clWindowText
  Font.Height = -11
  Font.Name = 'MS Sans Serif'
  Font.Style = []
  OldCreateOrder = False
  OnCreate = FormCreate
  PixelsPerInch = 96
  TextHeight = 13
  object Splitter1: TSplitter
    Left = 394
    Top = 41
    Width = 3
    Height = 351
    Cursor = crHSplit
    Align = alRight
  end
  object Memo1: TMemo
    Left = 0
    Top = 41
    Width = 394
    Height = 351
    Align = alClient
    TabOrder = 0
  end
  object users: TListBox
    Left = 397
    Top = 41
    Width = 121
    Height = 351
    Align = alRight
    ItemHeight = 13
    TabOrder = 1
  end
  object Panel1: TPanel
    Left = 0
    Top = 392
    Width = 518
    Height = 41
    Align = alBottom
    Caption = 'Panel1'
    TabOrder = 2
    object Edit1: TEdit
      Left = 8
      Top = 9
      Width = 417
      Height = 21
      TabOrder = 0
      OnKeyUp = Edit1KeyUp
    end
    object Button1: TButton
      Left = 432
      Top = 8
      Width = 75
      Height = 25
      Caption = 'Send'
      TabOrder = 1
      OnClick = Button1Click
    end
  end
  object Panel2: TPanel
    Left = 0
    Top = 0
    Width = 518
    Height = 41
    Align = alTop
    TabOrder = 3
    object Label1: TLabel
      Left = 8
      Top = 12
      Width = 20
      Height = 13
      Caption = 'user'
    end
    object Label2: TLabel
      Left = 208
      Top = 13
      Width = 29
      Height = 13
      Caption = 'server'
    end
    object Button2: TButton
      Left = 432
      Top = 8
      Width = 75
      Height = 25
      Caption = 'Connect'
      TabOrder = 0
      OnClick = Button2Click
    end
    object Edit2: TEdit
      Left = 40
      Top = 8
      Width = 121
      Height = 21
      TabOrder = 1
      Text = 'DelphiBinky'
      OnKeyUp = Edit2KeyUp
    end
    object Edit3: TEdit
      Left = 248
      Top = 8
      Width = 169
      Height = 21
      TabOrder = 2
      Text = 'beshare.tycomsystems.com'
    end
  end
  object MessageTransceiver1: TMessageTransceiver
    Host = 'beshare.tycomsystems.com'
    Port = 2960
    OnMessageReceived = MessageTransceiver1MessageReceived
    Left = 232
    Top = 160
  end
end
