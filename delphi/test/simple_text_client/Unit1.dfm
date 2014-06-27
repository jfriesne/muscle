object Form1: TForm1
  Left = 208
  Top = 190
  Width = 403
  Height = 289
  Caption = 'Simple text demo'
  Color = clBtnFace
  Font.Charset = DEFAULT_CHARSET
  Font.Color = clWindowText
  Font.Height = -11
  Font.Name = 'MS Sans Serif'
  Font.Style = []
  OldCreateOrder = False
  PixelsPerInch = 96
  TextHeight = 13
  object Button1: TButton
    Left = 8
    Top = 8
    Width = 75
    Height = 25
    Caption = 'Send'
    TabOrder = 0
    OnClick = Button1Click
  end
  object Edit1: TEdit
    Left = 88
    Top = 8
    Width = 289
    Height = 21
    TabOrder = 1
  end
  object Memo1: TMemo
    Left = 8
    Top = 40
    Width = 377
    Height = 185
    Lines.Strings = (
      '')
    TabOrder = 2
  end
  object Button2: TButton
    Left = 16
    Top = 232
    Width = 361
    Height = 25
    Caption = 'Connect'
    TabOrder = 3
    OnClick = Button2Click
  end
  object MessageTransceiver1: TMessageTransceiver
    Host = '127.0.0.1'
    Port = 2960
    OnMessageReceived = MessageTransceiver1MessageReceived
    Left = 112
    Top = 80
  end
end
