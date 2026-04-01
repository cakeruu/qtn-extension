export const QTN_TYPES = [
    'bool', 'Bool', 'Boolean',
    'byte', 'Byte',
    'sbyte', 'SByte',
    'short', 'Short',
    'ushort', 'UShort',
    'int', 'Int', 'Int32',
    'uint', 'UInt', 'UInt32',
    'long', 'Long',
    'ulong', 'ULong', 'UInt64',
    'FP', 'FPVector2', 'FPVector3', 'FPMatrix', 'FPQuaternion', 'LayerMask',
    'QString', 'QStringUtf8',
    'entity_ref', 'EntityRef',
    'player_ref', 'PlayerRef',
    'list', 'List',
    'dictionary', 'Dictionary',
    'hash_set', 'HashSet',
    'array', 'Array',
    'asset_ref', 'AssetRef',
    'button', 'Button'
] as const;

export type QTNType = typeof QTN_TYPES[number];

