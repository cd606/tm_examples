# -*- coding: utf-8 -*-
# Generated by the protocol buffer compiler.  DO NOT EDIT!
# source: defs.proto

from google.protobuf import descriptor as _descriptor
from google.protobuf import message as _message
from google.protobuf import reflection as _reflection
from google.protobuf import symbol_database as _symbol_database
# @@protoc_insertion_point(imports)

_sym_db = _symbol_database.Default()




DESCRIPTOR = _descriptor.FileDescriptor(
  name='defs.proto',
  package='simple_demo',
  syntax='proto3',
  serialized_options=None,
  create_key=_descriptor._internal_create_key,
  serialized_pb=b'\n\ndefs.proto\x12\x0bsimple_demo\"\x1a\n\tInputData\x12\r\n\x05value\x18\x01 \x01(\x01\"#\n\x10\x43onfigureCommand\x12\x0f\n\x07\x65nabled\x18\x01 \x01(\x08\"\"\n\x0f\x43onfigureResult\x12\x0f\n\x07\x65nabled\x18\x01 \x01(\x08\"-\n\x10\x43\x61lculateCommand\x12\n\n\x02id\x18\x01 \x01(\x05\x12\r\n\x05value\x18\x02 \x01(\x01\"-\n\x0f\x43\x61lculateResult\x12\n\n\x02id\x18\x01 \x01(\x05\x12\x0e\n\x06result\x18\x02 \x01(\x01\"\x1a\n\x18OutstandingCommandsQuery\"(\n\x19OutstandingCommandsResult\x12\x0b\n\x03ids\x18\x01 \x03(\x05\"\x1c\n\rClearCommands\x12\x0b\n\x03ids\x18\x01 \x03(\x05\"\"\n\x13\x43learCommandsResult\x12\x0b\n\x03ids\x18\x01 \x03(\x05\x62\x06proto3'
)




_INPUTDATA = _descriptor.Descriptor(
  name='InputData',
  full_name='simple_demo.InputData',
  filename=None,
  file=DESCRIPTOR,
  containing_type=None,
  create_key=_descriptor._internal_create_key,
  fields=[
    _descriptor.FieldDescriptor(
      name='value', full_name='simple_demo.InputData.value', index=0,
      number=1, type=1, cpp_type=5, label=1,
      has_default_value=False, default_value=float(0),
      message_type=None, enum_type=None, containing_type=None,
      is_extension=False, extension_scope=None,
      serialized_options=None, file=DESCRIPTOR,  create_key=_descriptor._internal_create_key),
  ],
  extensions=[
  ],
  nested_types=[],
  enum_types=[
  ],
  serialized_options=None,
  is_extendable=False,
  syntax='proto3',
  extension_ranges=[],
  oneofs=[
  ],
  serialized_start=27,
  serialized_end=53,
)


_CONFIGURECOMMAND = _descriptor.Descriptor(
  name='ConfigureCommand',
  full_name='simple_demo.ConfigureCommand',
  filename=None,
  file=DESCRIPTOR,
  containing_type=None,
  create_key=_descriptor._internal_create_key,
  fields=[
    _descriptor.FieldDescriptor(
      name='enabled', full_name='simple_demo.ConfigureCommand.enabled', index=0,
      number=1, type=8, cpp_type=7, label=1,
      has_default_value=False, default_value=False,
      message_type=None, enum_type=None, containing_type=None,
      is_extension=False, extension_scope=None,
      serialized_options=None, file=DESCRIPTOR,  create_key=_descriptor._internal_create_key),
  ],
  extensions=[
  ],
  nested_types=[],
  enum_types=[
  ],
  serialized_options=None,
  is_extendable=False,
  syntax='proto3',
  extension_ranges=[],
  oneofs=[
  ],
  serialized_start=55,
  serialized_end=90,
)


_CONFIGURERESULT = _descriptor.Descriptor(
  name='ConfigureResult',
  full_name='simple_demo.ConfigureResult',
  filename=None,
  file=DESCRIPTOR,
  containing_type=None,
  create_key=_descriptor._internal_create_key,
  fields=[
    _descriptor.FieldDescriptor(
      name='enabled', full_name='simple_demo.ConfigureResult.enabled', index=0,
      number=1, type=8, cpp_type=7, label=1,
      has_default_value=False, default_value=False,
      message_type=None, enum_type=None, containing_type=None,
      is_extension=False, extension_scope=None,
      serialized_options=None, file=DESCRIPTOR,  create_key=_descriptor._internal_create_key),
  ],
  extensions=[
  ],
  nested_types=[],
  enum_types=[
  ],
  serialized_options=None,
  is_extendable=False,
  syntax='proto3',
  extension_ranges=[],
  oneofs=[
  ],
  serialized_start=92,
  serialized_end=126,
)


_CALCULATECOMMAND = _descriptor.Descriptor(
  name='CalculateCommand',
  full_name='simple_demo.CalculateCommand',
  filename=None,
  file=DESCRIPTOR,
  containing_type=None,
  create_key=_descriptor._internal_create_key,
  fields=[
    _descriptor.FieldDescriptor(
      name='id', full_name='simple_demo.CalculateCommand.id', index=0,
      number=1, type=5, cpp_type=1, label=1,
      has_default_value=False, default_value=0,
      message_type=None, enum_type=None, containing_type=None,
      is_extension=False, extension_scope=None,
      serialized_options=None, file=DESCRIPTOR,  create_key=_descriptor._internal_create_key),
    _descriptor.FieldDescriptor(
      name='value', full_name='simple_demo.CalculateCommand.value', index=1,
      number=2, type=1, cpp_type=5, label=1,
      has_default_value=False, default_value=float(0),
      message_type=None, enum_type=None, containing_type=None,
      is_extension=False, extension_scope=None,
      serialized_options=None, file=DESCRIPTOR,  create_key=_descriptor._internal_create_key),
  ],
  extensions=[
  ],
  nested_types=[],
  enum_types=[
  ],
  serialized_options=None,
  is_extendable=False,
  syntax='proto3',
  extension_ranges=[],
  oneofs=[
  ],
  serialized_start=128,
  serialized_end=173,
)


_CALCULATERESULT = _descriptor.Descriptor(
  name='CalculateResult',
  full_name='simple_demo.CalculateResult',
  filename=None,
  file=DESCRIPTOR,
  containing_type=None,
  create_key=_descriptor._internal_create_key,
  fields=[
    _descriptor.FieldDescriptor(
      name='id', full_name='simple_demo.CalculateResult.id', index=0,
      number=1, type=5, cpp_type=1, label=1,
      has_default_value=False, default_value=0,
      message_type=None, enum_type=None, containing_type=None,
      is_extension=False, extension_scope=None,
      serialized_options=None, file=DESCRIPTOR,  create_key=_descriptor._internal_create_key),
    _descriptor.FieldDescriptor(
      name='result', full_name='simple_demo.CalculateResult.result', index=1,
      number=2, type=1, cpp_type=5, label=1,
      has_default_value=False, default_value=float(0),
      message_type=None, enum_type=None, containing_type=None,
      is_extension=False, extension_scope=None,
      serialized_options=None, file=DESCRIPTOR,  create_key=_descriptor._internal_create_key),
  ],
  extensions=[
  ],
  nested_types=[],
  enum_types=[
  ],
  serialized_options=None,
  is_extendable=False,
  syntax='proto3',
  extension_ranges=[],
  oneofs=[
  ],
  serialized_start=175,
  serialized_end=220,
)


_OUTSTANDINGCOMMANDSQUERY = _descriptor.Descriptor(
  name='OutstandingCommandsQuery',
  full_name='simple_demo.OutstandingCommandsQuery',
  filename=None,
  file=DESCRIPTOR,
  containing_type=None,
  create_key=_descriptor._internal_create_key,
  fields=[
  ],
  extensions=[
  ],
  nested_types=[],
  enum_types=[
  ],
  serialized_options=None,
  is_extendable=False,
  syntax='proto3',
  extension_ranges=[],
  oneofs=[
  ],
  serialized_start=222,
  serialized_end=248,
)


_OUTSTANDINGCOMMANDSRESULT = _descriptor.Descriptor(
  name='OutstandingCommandsResult',
  full_name='simple_demo.OutstandingCommandsResult',
  filename=None,
  file=DESCRIPTOR,
  containing_type=None,
  create_key=_descriptor._internal_create_key,
  fields=[
    _descriptor.FieldDescriptor(
      name='ids', full_name='simple_demo.OutstandingCommandsResult.ids', index=0,
      number=1, type=5, cpp_type=1, label=3,
      has_default_value=False, default_value=[],
      message_type=None, enum_type=None, containing_type=None,
      is_extension=False, extension_scope=None,
      serialized_options=None, file=DESCRIPTOR,  create_key=_descriptor._internal_create_key),
  ],
  extensions=[
  ],
  nested_types=[],
  enum_types=[
  ],
  serialized_options=None,
  is_extendable=False,
  syntax='proto3',
  extension_ranges=[],
  oneofs=[
  ],
  serialized_start=250,
  serialized_end=290,
)


_CLEARCOMMANDS = _descriptor.Descriptor(
  name='ClearCommands',
  full_name='simple_demo.ClearCommands',
  filename=None,
  file=DESCRIPTOR,
  containing_type=None,
  create_key=_descriptor._internal_create_key,
  fields=[
    _descriptor.FieldDescriptor(
      name='ids', full_name='simple_demo.ClearCommands.ids', index=0,
      number=1, type=5, cpp_type=1, label=3,
      has_default_value=False, default_value=[],
      message_type=None, enum_type=None, containing_type=None,
      is_extension=False, extension_scope=None,
      serialized_options=None, file=DESCRIPTOR,  create_key=_descriptor._internal_create_key),
  ],
  extensions=[
  ],
  nested_types=[],
  enum_types=[
  ],
  serialized_options=None,
  is_extendable=False,
  syntax='proto3',
  extension_ranges=[],
  oneofs=[
  ],
  serialized_start=292,
  serialized_end=320,
)


_CLEARCOMMANDSRESULT = _descriptor.Descriptor(
  name='ClearCommandsResult',
  full_name='simple_demo.ClearCommandsResult',
  filename=None,
  file=DESCRIPTOR,
  containing_type=None,
  create_key=_descriptor._internal_create_key,
  fields=[
    _descriptor.FieldDescriptor(
      name='ids', full_name='simple_demo.ClearCommandsResult.ids', index=0,
      number=1, type=5, cpp_type=1, label=3,
      has_default_value=False, default_value=[],
      message_type=None, enum_type=None, containing_type=None,
      is_extension=False, extension_scope=None,
      serialized_options=None, file=DESCRIPTOR,  create_key=_descriptor._internal_create_key),
  ],
  extensions=[
  ],
  nested_types=[],
  enum_types=[
  ],
  serialized_options=None,
  is_extendable=False,
  syntax='proto3',
  extension_ranges=[],
  oneofs=[
  ],
  serialized_start=322,
  serialized_end=356,
)

DESCRIPTOR.message_types_by_name['InputData'] = _INPUTDATA
DESCRIPTOR.message_types_by_name['ConfigureCommand'] = _CONFIGURECOMMAND
DESCRIPTOR.message_types_by_name['ConfigureResult'] = _CONFIGURERESULT
DESCRIPTOR.message_types_by_name['CalculateCommand'] = _CALCULATECOMMAND
DESCRIPTOR.message_types_by_name['CalculateResult'] = _CALCULATERESULT
DESCRIPTOR.message_types_by_name['OutstandingCommandsQuery'] = _OUTSTANDINGCOMMANDSQUERY
DESCRIPTOR.message_types_by_name['OutstandingCommandsResult'] = _OUTSTANDINGCOMMANDSRESULT
DESCRIPTOR.message_types_by_name['ClearCommands'] = _CLEARCOMMANDS
DESCRIPTOR.message_types_by_name['ClearCommandsResult'] = _CLEARCOMMANDSRESULT
_sym_db.RegisterFileDescriptor(DESCRIPTOR)

InputData = _reflection.GeneratedProtocolMessageType('InputData', (_message.Message,), {
  'DESCRIPTOR' : _INPUTDATA,
  '__module__' : 'defs_pb2'
  # @@protoc_insertion_point(class_scope:simple_demo.InputData)
  })
_sym_db.RegisterMessage(InputData)

ConfigureCommand = _reflection.GeneratedProtocolMessageType('ConfigureCommand', (_message.Message,), {
  'DESCRIPTOR' : _CONFIGURECOMMAND,
  '__module__' : 'defs_pb2'
  # @@protoc_insertion_point(class_scope:simple_demo.ConfigureCommand)
  })
_sym_db.RegisterMessage(ConfigureCommand)

ConfigureResult = _reflection.GeneratedProtocolMessageType('ConfigureResult', (_message.Message,), {
  'DESCRIPTOR' : _CONFIGURERESULT,
  '__module__' : 'defs_pb2'
  # @@protoc_insertion_point(class_scope:simple_demo.ConfigureResult)
  })
_sym_db.RegisterMessage(ConfigureResult)

CalculateCommand = _reflection.GeneratedProtocolMessageType('CalculateCommand', (_message.Message,), {
  'DESCRIPTOR' : _CALCULATECOMMAND,
  '__module__' : 'defs_pb2'
  # @@protoc_insertion_point(class_scope:simple_demo.CalculateCommand)
  })
_sym_db.RegisterMessage(CalculateCommand)

CalculateResult = _reflection.GeneratedProtocolMessageType('CalculateResult', (_message.Message,), {
  'DESCRIPTOR' : _CALCULATERESULT,
  '__module__' : 'defs_pb2'
  # @@protoc_insertion_point(class_scope:simple_demo.CalculateResult)
  })
_sym_db.RegisterMessage(CalculateResult)

OutstandingCommandsQuery = _reflection.GeneratedProtocolMessageType('OutstandingCommandsQuery', (_message.Message,), {
  'DESCRIPTOR' : _OUTSTANDINGCOMMANDSQUERY,
  '__module__' : 'defs_pb2'
  # @@protoc_insertion_point(class_scope:simple_demo.OutstandingCommandsQuery)
  })
_sym_db.RegisterMessage(OutstandingCommandsQuery)

OutstandingCommandsResult = _reflection.GeneratedProtocolMessageType('OutstandingCommandsResult', (_message.Message,), {
  'DESCRIPTOR' : _OUTSTANDINGCOMMANDSRESULT,
  '__module__' : 'defs_pb2'
  # @@protoc_insertion_point(class_scope:simple_demo.OutstandingCommandsResult)
  })
_sym_db.RegisterMessage(OutstandingCommandsResult)

ClearCommands = _reflection.GeneratedProtocolMessageType('ClearCommands', (_message.Message,), {
  'DESCRIPTOR' : _CLEARCOMMANDS,
  '__module__' : 'defs_pb2'
  # @@protoc_insertion_point(class_scope:simple_demo.ClearCommands)
  })
_sym_db.RegisterMessage(ClearCommands)

ClearCommandsResult = _reflection.GeneratedProtocolMessageType('ClearCommandsResult', (_message.Message,), {
  'DESCRIPTOR' : _CLEARCOMMANDSRESULT,
  '__module__' : 'defs_pb2'
  # @@protoc_insertion_point(class_scope:simple_demo.ClearCommandsResult)
  })
_sym_db.RegisterMessage(ClearCommandsResult)


# @@protoc_insertion_point(module_scope)