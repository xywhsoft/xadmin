


// ============================================
// 模型字段类型定义
// ============================================



// 字段数据类型（数据库存储）
typedef enum {
	FIELD_TYPE_TEXT = 0,		// 文本 TEXT
	FIELD_TYPE_INTEGER,			// 整数 INTEGER
	FIELD_TYPE_FLOAT,			// 浮点 REAL
	FIELD_TYPE_BLOB,			// 二进制 BLOB
	FIELD_TYPE_JSON,			// JSON文本 TEXT
} FieldDataType;



// 字段表单类型（前端渲染）
typedef enum {
	FORM_TYPE_INPUT = 0,		// 单行文本
	FORM_TYPE_TEXTAREA,			// 多行文本
	FORM_TYPE_NUMBER,			// 数字输入
	FORM_TYPE_PASSWORD,			// 密码
	FORM_TYPE_SELECT,			// 下拉选择
	FORM_TYPE_RADIO,			// 单选
	FORM_TYPE_CHECKBOX,			// 多选
	FORM_TYPE_SWITCH,			// 开关
	FORM_TYPE_DATE,				// 日期
	FORM_TYPE_DATETIME,			// 日期时间
	FORM_TYPE_IMAGE,			// 图片上传
	FORM_TYPE_IMAGES,			// 多图上传
	FORM_TYPE_FILE,				// 文件上传
	FORM_TYPE_FILES,			// 多文件上传
	FORM_TYPE_RICHTEXT,			// 富文本
	FORM_TYPE_MARKDOWN,			// Markdown
	FORM_TYPE_COLOR,			// 颜色选择
	FORM_TYPE_ICON,				// 图标选择
	FORM_TYPE_HIDDEN,			// 隐藏字段
} FieldFormType;



// 字段类型信息结构
typedef struct {
	int iType;					// 类型ID
	str sName;					// 类型标识（英文）
	str sTitle;					// 类型名称（中文）
	str sDbType;				// 对应数据库类型
} FieldTypeInfo;



// 表单类型信息结构
typedef struct {
	int iType;					// 类型ID
	str sName;					// 类型标识（英文）
	str sTitle;					// 类型名称（中文）
	str sDefaultDbType;			// 默认数据库类型
} FormTypeInfo;



// 字段数据类型注册表
FieldTypeInfo G_FieldTypes[] = {
	{ FIELD_TYPE_TEXT,		"text",		"文本",		"TEXT" },
	{ FIELD_TYPE_INTEGER,	"integer",	"整数",		"INTEGER" },
	{ FIELD_TYPE_FLOAT,		"float",	"浮点数",	"REAL" },
	{ FIELD_TYPE_BLOB,		"blob",		"二进制",	"BLOB" },
	{ FIELD_TYPE_JSON,		"json",		"JSON",		"TEXT" },
};
int G_FieldTypesCount = sizeof(G_FieldTypes) / sizeof(FieldTypeInfo);



// 表单类型注册表
FormTypeInfo G_FormTypes[] = {
	{ FORM_TYPE_INPUT,		"input",		"单行文本",		"TEXT" },
	{ FORM_TYPE_TEXTAREA,	"textarea",		"多行文本",		"TEXT" },
	{ FORM_TYPE_NUMBER,		"number",		"数字",			"INTEGER" },
	{ FORM_TYPE_PASSWORD,	"password",		"密码",			"TEXT" },
	{ FORM_TYPE_SELECT,		"select",		"下拉选择",		"TEXT" },
	{ FORM_TYPE_RADIO,		"radio",		"单选",			"TEXT" },
	{ FORM_TYPE_CHECKBOX,	"checkbox",		"多选",			"TEXT" },
	{ FORM_TYPE_SWITCH,		"switch",		"开关",			"INTEGER" },
	{ FORM_TYPE_DATE,		"date",			"日期",			"INTEGER" },
	{ FORM_TYPE_DATETIME,	"datetime",		"日期时间",		"INTEGER" },
	{ FORM_TYPE_IMAGE,		"image",		"图片上传",		"TEXT" },
	{ FORM_TYPE_IMAGES,		"images",		"多图上传",		"TEXT" },
	{ FORM_TYPE_FILE,		"file",			"文件上传",		"TEXT" },
	{ FORM_TYPE_FILES,		"files",		"多文件上传",	"TEXT" },
	{ FORM_TYPE_RICHTEXT,	"richtext",		"富文本",		"TEXT" },
	{ FORM_TYPE_MARKDOWN,	"markdown",		"Markdown",		"TEXT" },
	{ FORM_TYPE_COLOR,		"color",		"颜色选择",		"TEXT" },
	{ FORM_TYPE_ICON,		"icon",			"图标选择",		"TEXT" },
	{ FORM_TYPE_HIDDEN,		"hidden",		"隐藏字段",		"TEXT" },
};
int G_FormTypesCount = sizeof(G_FormTypes) / sizeof(FormTypeInfo);



// 根据名称获取字段数据类型
FieldTypeInfo* ModelField_GetFieldType(str sName)
{
	for ( int i = 0; i < G_FieldTypesCount; i++ ) {
		if ( strcmp(G_FieldTypes[i].sName, sName) == 0 ) {
			return &G_FieldTypes[i];
		}
	}
	return NULL;
}



// 根据名称获取表单类型
FormTypeInfo* ModelField_GetFormType(str sName)
{
	for ( int i = 0; i < G_FormTypesCount; i++ ) {
		if ( strcmp(G_FormTypes[i].sName, sName) == 0 ) {
			return &G_FormTypes[i];
		}
	}
	return NULL;
}



// 获取字段数据类型列表（用于前端渲染）
xvalue ModelField_GetFieldTypeList()
{
	xvalue arrList = xvoCreateArray();
	for ( int i = 0; i < G_FieldTypesCount; i++ ) {
		xvalue tblItem = xvoCreateTable();
		xvoTableSetInt(tblItem, "value", 5, G_FieldTypes[i].iType);
		xvoTableSetText(tblItem, "name", 4, G_FieldTypes[i].sName, 0, FALSE);
		xvoTableSetText(tblItem, "label", 5, G_FieldTypes[i].sTitle, 0, FALSE);
		xvoTableSetText(tblItem, "dbType", 6, G_FieldTypes[i].sDbType, 0, FALSE);
		xvoArrayAppendValue(arrList, tblItem, TRUE);
	}
	return arrList;
}



// 获取表单类型列表（用于前端渲染）
xvalue ModelField_GetFormTypeList()
{
	xvalue arrList = xvoCreateArray();
	for ( int i = 0; i < G_FormTypesCount; i++ ) {
		xvalue tblItem = xvoCreateTable();
		xvoTableSetInt(tblItem, "value", 5, G_FormTypes[i].iType);
		xvoTableSetText(tblItem, "name", 4, G_FormTypes[i].sName, 0, FALSE);
		xvoTableSetText(tblItem, "label", 5, G_FormTypes[i].sTitle, 0, FALSE);
		xvoTableSetText(tblItem, "defaultDbType", 13, G_FormTypes[i].sDefaultDbType, 0, FALSE);
		xvoArrayAppendValue(arrList, tblItem, TRUE);
	}
	return arrList;
}


