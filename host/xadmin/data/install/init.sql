--
-- xAdmin 数据库初始化脚本
-- 整理日期: 2026-01-09
--
-- 所用的文本编码：UTF-8
--
PRAGMA foreign_keys = off;
BEGIN TRANSACTION;

-- ============================================================
-- 表结构定义
-- ============================================================

-- 表：attachment - 附件存储
CREATE TABLE attachment (
	xid           TEXT    PRIMARY KEY,
	filename      TEXT,
	ext           TEXT,
	mime          TEXT,
	size          INTEGER DEFAULT (0),
	path          TEXT,
	modelName     TEXT    DEFAULT '',
	recordId      INTEGER DEFAULT (0),
	uploaderId    INTEGER DEFAULT (0),
	uploaderType  INTEGER DEFAULT (1),
	allowHotlink  INTEGER DEFAULT (1),
	accessType    INTEGER DEFAULT (0),
	accessLevel   INTEGER DEFAULT (0),
	price         INTEGER DEFAULT (0),
	priceType     INTEGER DEFAULT (0),
	salesCount    INTEGER DEFAULT (0),
	downloadCount INTEGER DEFAULT (0),
	remark        TEXT    DEFAULT '',
	createTime    INTEGER,
	isDelete      INTEGER DEFAULT (0) 
);

-- 表：attachmentOrder - 附件购买订单
CREATE TABLE attachmentOrder (
	id            INTEGER PRIMARY KEY AUTOINCREMENT,
	attachmentXid TEXT,
	memberId      INTEGER,
	price         INTEGER DEFAULT (0),
	priceType     INTEGER DEFAULT (0),
	sellerId      INTEGER DEFAULT (0),
	sellerIncome  INTEGER DEFAULT (0),
	createTime    INTEGER
);

-- 表：auth - 权限分组
CREATE TABLE auth (
	id         INTEGER PRIMARY KEY AUTOINCREMENT,
	groupID    INTEGER,
	name       TEXT,
	desc       TEXT,
	sort       INTEGER,
	createTime INTEGER,
	updateTime INTEGER,
	isDelete   INTEGER DEFAULT (0) 
);

-- 表：authGroup - 权限分类
CREATE TABLE authGroup (
	id         INTEGER PRIMARY KEY AUTOINCREMENT,
	name       TEXT,
	desc       TEXT,
	sort       INTEGER,
	createTime INTEGER,
	updateTime INTEGER,
	isDelete   INTEGER DEFAULT (0) 
);

-- 表：logs - 系统日志
CREATE TABLE logs (
	id         INTEGER PRIMARY KEY AUTOINCREMENT,
	user       TEXT,
	ip         TEXT,
	uri        TEXT,
	method     TEXT,
	param      TEXT,
	body       TEXT,
	createTime INTEGER
);

-- 表：member - 前台用户
CREATE TABLE member (
	id         INTEGER PRIMARY KEY AUTOINCREMENT,
	username   TEXT    UNIQUE,
	salt       TEXT,
	pwd        TEXT,
	groupId    INTEGER DEFAULT (1),
	authLevel  INTEGER DEFAULT (0),
	balance    INTEGER DEFAULT (0),
	nickname   TEXT,
	email      TEXT,
	phone      TEXT,
	avatar     TEXT,
	status     INTEGER DEFAULT (1),
	createTime INTEGER,
	updateTime INTEGER,
	isDelete   INTEGER DEFAULT (0) 
);

-- 表：memberAuth - 前台权限分组
CREATE TABLE memberAuth (
	id         INTEGER PRIMARY KEY AUTOINCREMENT,
	groupID    INTEGER DEFAULT (1),
	name       TEXT,
	desc       TEXT,
	sort       INTEGER DEFAULT (0),
	createTime INTEGER,
	updateTime INTEGER,
	isDelete   INTEGER DEFAULT (0) 
);

-- 表：memberAuthGroup - 前台权限分类
CREATE TABLE memberAuthGroup (
	id         INTEGER PRIMARY KEY AUTOINCREMENT,
	name       TEXT,
	desc       TEXT,
	sort       INTEGER DEFAULT (0),
	createTime INTEGER,
	updateTime INTEGER,
	isDelete   INTEGER DEFAULT (0) 
);

-- 表：memberBalanceLog - 前台用户余额日志
CREATE TABLE memberBalanceLog (
	id         INTEGER PRIMARY KEY AUTOINCREMENT,
	memberId   INTEGER,
	type       INTEGER DEFAULT (0),
	amount     INTEGER DEFAULT (0),
	balance    INTEGER DEFAULT (0),
	remark     TEXT,
	operator   TEXT,
	createTime INTEGER
);

-- 表：memberGroup - 前台用户组
CREATE TABLE memberGroup (
	id         INTEGER PRIMARY KEY AUTOINCREMENT,
	name       TEXT,
	desc       TEXT,
	authList   TEXT    DEFAULT '[]',
	authLevel  INTEGER DEFAULT (0),
	createTime INTEGER,
	updateTime INTEGER,
	isDelete   INTEGER DEFAULT (0) 
);

-- 表：menu - 后台菜单
CREATE TABLE menu (
	id         INTEGER PRIMARY KEY AUTOINCREMENT,
	parent     INTEGER DEFAULT (0),
	title      TEXT,
	icon       TEXT,
	type       INTEGER DEFAULT (1),
	openType   TEXT    DEFAULT '_component',
	href       TEXT,
	sort       INTEGER DEFAULT (0),
	visible    INTEGER DEFAULT (1),
	remark     TEXT,
	createTime INTEGER,
	updateTime INTEGER,
	isDelete   INTEGER DEFAULT (0) 
);

-- 表：reply - 评论回复
CREATE TABLE reply (
	id         INTEGER PRIMARY KEY AUTOINCREMENT,
	modelName  TEXT,
	contentId  INTEGER,
	userId     INTEGER,
	userType   INTEGER DEFAULT (0),
	content    TEXT,
	quoteId    INTEGER DEFAULT (0),
	quoteText  TEXT,
	status     INTEGER DEFAULT (1),
	createTime INTEGER,
	updateTime INTEGER,
	isDelete   INTEGER DEFAULT (0) 
);

-- 表：role - 后台角色
CREATE TABLE role (
	id         INTEGER PRIMARY KEY AUTOINCREMENT,
	name       TEXT,
	desc       TEXT,
	authList   TEXT    DEFAULT "[]",
	authLevel  INTEGER DEFAULT (0),
	createTime INTEGER,
	updateTime INTEGER,
	isDelete   INTEGER
);

-- 表：uris - URI 权限配置
CREATE TABLE uris (
	id         INTEGER PRIMARY KEY AUTOINCREMENT UNIQUE,
	authID     INTEGER REFERENCES auth (id),
	uri        TEXT    UNIQUE,
	desc       TEXT,
	isBackend  INTEGER DEFAULT 1,
	needAuth   INTEGER DEFAULT 1,
	needLog    INTEGER DEFAULT 0,
	keepActive INTEGER DEFAULT 0,
	sort       INTEGER,
	createTime INTEGER,
	updateTime INTEGER
);

-- 表：user - 后台用户
CREATE TABLE user (
	id         INTEGER PRIMARY KEY AUTOINCREMENT,
	user       TEXT,
	salt       TEXT,
	pwd        TEXT,
	role       INTEGER,
	authLevel  INTEGER DEFAULT (0),
	createTime INTEGER,
	updateTime INTEGER,
	isDelete   INTEGER
);


-- ============================================================
-- authGroup 权限分类数据
-- ============================================================
-- 排序规则: 从小到大排列, 每个功能模块 100000
-- id=1 未分类, id=2 基础权限, id=3 内容模型, id=4 附件管理
-- id=5 前台用户管理, id=6 后台权限管理, id=7 设置, id=8 系统日志, id=9 调试接口

INSERT INTO authGroup (id, name, desc, sort, createTime, updateTime, isDelete)
VALUES (1, '未分类', '未分类的权限组会归到这里', 0, 63930028060, 63936000000, 0);

INSERT INTO authGroup (id, name, desc, sort, createTime, updateTime, isDelete)
VALUES (2, '基础权限', '后台登录、主页等基础功能', 10000, 63933903097, 63936000000, 0);

INSERT INTO authGroup (id, name, desc, sort, createTime, updateTime, isDelete)
VALUES (3, '内容模型', '[内容模型] 菜单下的功能', 100000, 63936000000, 63936000000, 0);

INSERT INTO authGroup (id, name, desc, sort, createTime, updateTime, isDelete)
VALUES (4, '附件管理', '[附件管理] 菜单下的功能', 200000, 63936000000, 63936000000, 0);

INSERT INTO authGroup (id, name, desc, sort, createTime, updateTime, isDelete)
VALUES (5, '前台用户管理', '[前台用户管理] 菜单下的功能', 300000, 63934948299, 63936000000, 0);

INSERT INTO authGroup (id, name, desc, sort, createTime, updateTime, isDelete)
VALUES (6, '后台权限管理', '[后台权限管理] 菜单下的功能', 400000, 63933903052, 63936000000, 0);

INSERT INTO authGroup (id, name, desc, sort, createTime, updateTime, isDelete)
VALUES (7, '设置', '[设置] 菜单下的功能', 500000, 63934948202, 63936000000, 0);

INSERT INTO authGroup (id, name, desc, sort, createTime, updateTime, isDelete)
VALUES (8, '系统日志', '[系统日志] 菜单下的功能', 600000, 63933903025, 63936000000, 0);

INSERT INTO authGroup (id, name, desc, sort, createTime, updateTime, isDelete)
VALUES (9, '调试接口', '用于开发调试的接口', 700000, 63934949629, 63936000000, 0);


-- ============================================================
-- auth 权限分组数据
-- ============================================================
-- 排序规则: 功能模块基础值 + 二级功能编号*1000
-- groupID 关联 authGroup.id

-- 未分类 (groupID=1)
INSERT INTO auth (id, groupID, name, desc, sort, createTime, updateTime, isDelete)
VALUES (1, 1, '未分组', '未分组的 URI 会被归类到这里', 0, 63930019316, 63936000000, 0);

-- 基础权限 (groupID=2)
INSERT INTO auth (id, groupID, name, desc, sort, createTime, updateTime, isDelete)
VALUES (2, 2, '基础页面', '后台登录、主页等基础功能页面和接口', 10000, 63933903210, 63936000000, 0);

INSERT INTO auth (id, groupID, name, desc, sort, createTime, updateTime, isDelete)
VALUES (3, 2, '前台基础接口', '前台用户登录、注册、个人中心等基础接口', 10100, 63936000000, 63936000000, 0);

-- 内容模型 (groupID=3)
INSERT INTO auth (id, groupID, name, desc, sort, createTime, updateTime, isDelete)
VALUES (4, 3, '模型管理', '[内容模型] 模型列表、创建、编辑、删除等', 100000, 63936000000, 63936000000, 0);

-- 附件管理 (groupID=4)
INSERT INTO auth (id, groupID, name, desc, sort, createTime, updateTime, isDelete)
VALUES (5, 4, '附件列表', '[附件管理] - [附件列表] 页面和接口', 200000, 63936000000, 63936000000, 0);

INSERT INTO auth (id, groupID, name, desc, sort, createTime, updateTime, isDelete)
VALUES (6, 4, '存储统计', '[附件管理] - [存储统计] 页面和接口', 201000, 63936000000, 63936000000, 0);

INSERT INTO auth (id, groupID, name, desc, sort, createTime, updateTime, isDelete)
VALUES (7, 4, '前台附件接口', '前台附件上传、购买、查询等接口', 202000, 63936000000, 63936000000, 0);

-- 前台用户管理 (groupID=5)
INSERT INTO auth (id, groupID, name, desc, sort, createTime, updateTime, isDelete)
VALUES (8, 5, '用户管理', '[前台用户管理] - [用户管理] 页面和接口', 300000, 63934949282, 63936000000, 0);

INSERT INTO auth (id, groupID, name, desc, sort, createTime, updateTime, isDelete)
VALUES (9, 5, '用户组管理', '[前台用户管理] - [用户组管理] 页面和接口', 301000, 63934949319, 63936000000, 0);

INSERT INTO auth (id, groupID, name, desc, sort, createTime, updateTime, isDelete)
VALUES (10, 5, '权限分类', '[前台用户管理] - [权限分类] 页面和接口', 302000, 63934949844, 63936000000, 0);

INSERT INTO auth (id, groupID, name, desc, sort, createTime, updateTime, isDelete)
VALUES (11, 5, '权限分组', '[前台用户管理] - [权限分组] 页面和接口', 303000, 63934949844, 63936000000, 0);

-- 后台权限管理 (groupID=6)
INSERT INTO auth (id, groupID, name, desc, sort, createTime, updateTime, isDelete)
VALUES (12, 6, '用户管理', '[后台权限管理] - [用户管理] 页面和接口', 400000, 63933903342, 63936000000, 0);

INSERT INTO auth (id, groupID, name, desc, sort, createTime, updateTime, isDelete)
VALUES (13, 6, '角色管理', '[后台权限管理] - [角色管理] 页面和接口', 401000, 63933903366, 63936000000, 0);

INSERT INTO auth (id, groupID, name, desc, sort, createTime, updateTime, isDelete)
VALUES (14, 6, '权限分类', '[后台权限管理] - [权限分类] 页面和接口', 402000, 63933903387, 63936000000, 0);

INSERT INTO auth (id, groupID, name, desc, sort, createTime, updateTime, isDelete)
VALUES (15, 6, '权限分组', '[后台权限管理] - [权限分组] 页面和接口', 403000, 63933903428, 63936000000, 0);

-- 设置 (groupID=7)
INSERT INTO auth (id, groupID, name, desc, sort, createTime, updateTime, isDelete)
VALUES (16, 7, '设置管理', '[设置] 自定义配置页面和接口', 500000, 63934948478, 63936000000, 0);

INSERT INTO auth (id, groupID, name, desc, sort, createTime, updateTime, isDelete)
VALUES (17, 7, '菜单管理', '[设置] - [菜单管理] 页面和接口', 501000, 63934948510, 63936000000, 0);

INSERT INTO auth (id, groupID, name, desc, sort, createTime, updateTime, isDelete)
VALUES (18, 7, '接口管理', '[设置] - [接口管理] 页面和接口', 502000, 63933903456, 63936000000, 0);

-- 系统日志 (groupID=8)
INSERT INTO auth (id, groupID, name, desc, sort, createTime, updateTime, isDelete)
VALUES (19, 8, '系统日志', '[系统日志] 页面和接口', 600000, 63933903308, 63936000000, 0);

-- 调试接口 (groupID=9)
INSERT INTO auth (id, groupID, name, desc, sort, createTime, updateTime, isDelete)
VALUES (20, 9, '调试接口', '用于开发调试的接口', 700000, 63934949645, 63936000000, 0);


-- ============================================================
-- menu 菜单数据
-- ============================================================
-- 排序规则: 功能模块基础值 + 二级菜单编号*100
-- type: 0=目录, 1=菜单项

-- 内容模型 (sort=100000)
INSERT INTO menu (id, parent, title, icon, type, openType, href, sort, visible, remark, createTime, updateTime, isDelete)
VALUES (1, 0, '内容模型', 'layui-icon layui-icon-component', 1, '_component', '/admin/view/model', 100000, 1, '内容模型管理，可定义自定义内容结构', 63935107931, 63936000000, 0);

-- 附件管理 (sort=200000)
INSERT INTO menu (id, parent, title, icon, type, openType, href, sort, visible, remark, createTime, updateTime, isDelete)
VALUES (2, 0, '附件管理', 'layui-icon layui-icon-upload-drag', 0, '', '', 200000, 1, '附件管理目录', 63935100000, 63936000000, 0);

INSERT INTO menu (id, parent, title, icon, type, openType, href, sort, visible, remark, createTime, updateTime, isDelete)
VALUES (3, 2, '附件列表', 'layui-icon layui-icon-file', 1, '_component', '/admin/view/attachment', 200100, 1, '管理所有附件', 63935100000, 63936000000, 0);

INSERT INTO menu (id, parent, title, icon, type, openType, href, sort, visible, remark, createTime, updateTime, isDelete)
VALUES (4, 2, '存储统计', 'layui-icon layui-icon-chart', 1, '_component', '/admin/view/attachment/stats', 200200, 1, '查看存储统计信息', 63935100000, 63936000000, 0);

-- 前台用户管理 (sort=300000)
INSERT INTO menu (id, parent, title, icon, type, openType, href, sort, visible, remark, createTime, updateTime, isDelete)
VALUES (5, 0, '前台用户管理', 'layui-icon layui-icon-friends', 0, '', '', 300000, 1, '前台用户管理目录', 63934949844, 63936000000, 0);

INSERT INTO menu (id, parent, title, icon, type, openType, href, sort, visible, remark, createTime, updateTime, isDelete)
VALUES (6, 5, '用户管理', 'layui-icon layui-icon-username', 1, '_component', '/admin/view/member/user', 300100, 1, '管理前台用户', 63934949844, 63936000000, 0);

INSERT INTO menu (id, parent, title, icon, type, openType, href, sort, visible, remark, createTime, updateTime, isDelete)
VALUES (7, 5, '用户组管理', 'layui-icon layui-icon-group', 1, '_component', '/admin/view/member/group', 300200, 1, '管理前台用户组', 63934949844, 63936000000, 0);

INSERT INTO menu (id, parent, title, icon, type, openType, href, sort, visible, remark, createTime, updateTime, isDelete)
VALUES (8, 5, '权限分类', 'layui-icon layui-icon-tabs', 1, '_component', '/admin/view/member/authgroup', 300300, 1, '管理前台权限分类', 63934949844, 63936000000, 0);

INSERT INTO menu (id, parent, title, icon, type, openType, href, sort, visible, remark, createTime, updateTime, isDelete)
VALUES (9, 5, '权限分组', 'layui-icon layui-icon-vercode', 1, '_component', '/admin/view/member/auth', 300400, 1, '管理前台权限分组', 63934949844, 63936000000, 0);

-- 后台权限管理 (sort=400000)
INSERT INTO menu (id, parent, title, icon, type, openType, href, sort, visible, remark, createTime, updateTime, isDelete)
VALUES (10, 0, '后台权限管理', 'layui-icon layui-icon-auz', 0, '', '', 400000, 1, '后台权限管理目录', 63933640350, 63936000000, 0);

INSERT INTO menu (id, parent, title, icon, type, openType, href, sort, visible, remark, createTime, updateTime, isDelete)
VALUES (11, 10, '用户管理', 'layui-icon layui-icon-username', 1, '_component', '/admin/view/auth/user', 400100, 1, '管理后台用户', 63933640350, 63936000000, 0);

INSERT INTO menu (id, parent, title, icon, type, openType, href, sort, visible, remark, createTime, updateTime, isDelete)
VALUES (12, 10, '角色管理', 'layui-icon layui-icon-user', 1, '_component', '/admin/view/auth/role', 400200, 1, '管理用户角色', 63933640350, 63936000000, 0);

INSERT INTO menu (id, parent, title, icon, type, openType, href, sort, visible, remark, createTime, updateTime, isDelete)
VALUES (13, 10, '权限分类', 'layui-icon layui-icon-tabs', 1, '_component', '/admin/view/auth/group', 400300, 1, '管理权限分类', 63933640350, 63936000000, 0);

INSERT INTO menu (id, parent, title, icon, type, openType, href, sort, visible, remark, createTime, updateTime, isDelete)
VALUES (14, 10, '权限分组', 'layui-icon layui-icon-template', 1, '_component', '/admin/view/auth/auth', 400400, 1, '管理权限分组', 63933640350, 63936000000, 0);

-- 设置 (sort=500000)
INSERT INTO menu (id, parent, title, icon, type, openType, href, sort, visible, remark, createTime, updateTime, isDelete)
VALUES (15, 0, '设置', 'layui-icon layui-icon-set', 0, '', '', 500000, 1, '设置目录', 63934847322, 63936000000, 0);

INSERT INTO menu (id, parent, title, icon, type, openType, href, sort, visible, remark, createTime, updateTime, isDelete)
VALUES (16, 15, '全局配置', 'layui-icon layui-icon-set', 1, '_iframe', '/admin/view/option?file=global.json', 500100, 1, '系统全局配置管理', 63933640350, 63936000000, 0);

INSERT INTO menu (id, parent, title, icon, type, openType, href, sort, visible, remark, createTime, updateTime, isDelete)
VALUES (17, 15, '自定义设置', 'layui-icon layui-icon-face-smile', 1, '_iframe', '/admin/view/option?file=my.json', 500200, 1, '自定义配置项管理', 63934855728, 63936000000, 0);

INSERT INTO menu (id, parent, title, icon, type, openType, href, sort, visible, remark, createTime, updateTime, isDelete)
VALUES (18, 15, '附件设置', 'layui-icon layui-icon-upload', 1, '_iframe', '/admin/view/option?file=attachment.json', 500300, 1, '附件上传和存储配置', 63935205971, 63936000000, 0);

INSERT INTO menu (id, parent, title, icon, type, openType, href, sort, visible, remark, createTime, updateTime, isDelete)
VALUES (19, 15, '菜单管理', 'layui-icon layui-icon-spread-left', 1, '_component', '/admin/view/option/menu', 500400, 1, '管理系统菜单', 63933640350, 63936000000, 0);

INSERT INTO menu (id, parent, title, icon, type, openType, href, sort, visible, remark, createTime, updateTime, isDelete)
VALUES (20, 15, '接口管理', 'layui-icon layui-icon-website', 1, '_component', '/admin/view/auth/uris', 500500, 1, '管理API接口', 63933640350, 63936000000, 0);

INSERT INTO menu (id, parent, title, icon, type, openType, href, sort, visible, remark, createTime, updateTime, isDelete)
VALUES (22, 0, '插件管理', 'layui-icon layui-icon-app', 0, '', '', 550000, 1, '插件管理目录', 63936000000, 63936000000, 0);

INSERT INTO menu (id, parent, title, icon, type, openType, href, sort, visible, remark, createTime, updateTime, isDelete)
VALUES (23, 22, '插件商店', 'layui-icon layui-icon-cart-simple', 1, '_component', '/admin/view/plugin/store', 550100, 1, '浏览远程插件商店', 63936000000, 63936000000, 0);

INSERT INTO menu (id, parent, title, icon, type, openType, href, sort, visible, remark, createTime, updateTime, isDelete)
VALUES (24, 22, '已安装插件', 'layui-icon layui-icon-component', 1, '_component', '/admin/view/plugin/installed', 550200, 1, '查看和管理已安装插件', 63936000000, 63936000000, 0);

-- 系统日志 (sort=600000)
INSERT INTO menu (id, parent, title, icon, type, openType, href, sort, visible, remark, createTime, updateTime, isDelete)
VALUES (21, 0, '系统日志', 'layui-icon layui-icon-log', 1, '_component', '/admin/view/logs', 600000, 1, '查看系统日志', 63933640350, 63936000000, 0);


-- ============================================================
-- role 角色数据
-- ============================================================
-- authList 关联 auth.id

INSERT INTO role (id, name, desc, authList, authLevel, createTime, updateTime, isDelete)
VALUES (1, '超级管理员', '超级管理员账户，拥有后台的全部权限', '[1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20]', 999, 63930031353, 63936000000, 0);

INSERT INTO role (id, name, desc, authList, authLevel, createTime, updateTime, isDelete)
VALUES (2, '管理员', '拥有除权限管理外的大部分权限', '[2,3,4,5,6,7,8,9,10,11,16,17,19]', 200, 63930031443, 63936000000, 0);


-- ============================================================
-- user 后台用户数据
-- ============================================================

INSERT INTO user (id, user, salt, pwd, role, authLevel, createTime, updateTime, isDelete)
VALUES (1, 'admin', 'yT30uWu00001a1EiofyV4b9-_aHNO0aS', '4FD681049123E6D9E48D534D7AA7DACEDE38C8860B14069647FF1E4446573521', 1, 0, 63933640350, 63933829361, 0);


-- ============================================================
-- memberAuthGroup 前台权限分类数据
-- ============================================================

INSERT INTO memberAuthGroup (id, name, desc, sort, createTime, updateTime, isDelete)
VALUES (1, '未分类', '未分类的权限', 0, 63935000000, 63936000000, 0);

INSERT INTO memberAuthGroup (id, name, desc, sort, createTime, updateTime, isDelete)
VALUES (2, '基础功能', '基础访问权限', 100, 63935000000, 63936000000, 0);


-- ============================================================
-- memberAuth 前台权限分组数据
-- ============================================================

INSERT INTO memberAuth (id, groupID, name, desc, sort, createTime, updateTime, isDelete)
VALUES (1, 2, '基础接口', '登录、注册、个人中心等基础接口', 0, 63935000000, 63936000000, 0);

INSERT INTO memberAuth (id, groupID, name, desc, sort, createTime, updateTime, isDelete)
VALUES (2, 2, 'VIP功能', 'VIP专属功能接口', 100, 63935000000, 63936000000, 0);


-- ============================================================
-- memberGroup 前台用户组数据
-- ============================================================

INSERT INTO memberGroup (id, name, desc, authList, authLevel, createTime, updateTime, isDelete)
VALUES (1, '普通用户', '默认用户组，拥有基础访问权限', '[1]', 0, 63935000000, 63936000000, 0);

INSERT INTO memberGroup (id, name, desc, authList, authLevel, createTime, updateTime, isDelete)
VALUES (2, 'VIP用户', 'VIP用户组，拥有更多访问权限', '[1,2]', 200, 63935000000, 63936000000, 0);


-- ============================================================
-- uris URI权限数据
-- ============================================================
-- 排序规则: 后台URI = 1000000 + 功能基础值 + 二级*100 + 序号
--          前台URI = 功能基础值 + 二级*100 + 序号
-- authID 关联 auth.id

-- ========== 基础权限 - 后台基础页面 (authID=2) ==========
INSERT INTO uris (id, authID, uri, desc, isBackend, needAuth, needLog, keepActive, sort, createTime, updateTime)
VALUES (1, 2, '/admin/login', '后台 [用户登录] 主接口', 1, 0, 1, 0, 1010000, 63935023250, 63936000000);

INSERT INTO uris (id, authID, uri, desc, isBackend, needAuth, needLog, keepActive, sort, createTime, updateTime)
VALUES (2, 2, '/admin', '后台管理首页入口', 1, 1, 0, 1, 1010001, 63933638061, 63936000000);

INSERT INTO uris (id, authID, uri, desc, isBackend, needAuth, needLog, keepActive, sort, createTime, updateTime)
VALUES (3, 2, '/admin/view/home', '后台主页视图', 1, 1, 0, 1, 1010002, 63933638061, 63936000000);

INSERT INTO uris (id, authID, uri, desc, isBackend, needAuth, needLog, keepActive, sort, createTime, updateTime)
VALUES (4, 2, '/admin/menu', '后台左侧菜单数据接口', 1, 1, 0, 0, 1010003, 63933645834, 63936000000);

INSERT INTO uris (id, authID, uri, desc, isBackend, needAuth, needLog, keepActive, sort, createTime, updateTime)
VALUES (5, 2, '/admin/logout', '后台 [注销登录] 接口', 1, 1, 0, 0, 1010004, 63933638061, 63936000000);

-- ========== 基础权限 - 前台基础接口 (authID=3) ==========
INSERT INTO uris (id, authID, uri, desc, isBackend, needAuth, needLog, keepActive, sort, createTime, updateTime)
VALUES (6, 3, '/api/v1/register', '前台 [用户注册] 接口', 0, 0, 0, 0, 10100, 63935023250, 63936000000);

INSERT INTO uris (id, authID, uri, desc, isBackend, needAuth, needLog, keepActive, sort, createTime, updateTime)
VALUES (7, 3, '/api/v1/login', '前台 [用户登录] 接口', 0, 0, 0, 0, 10101, 63935023250, 63936000000);

INSERT INTO uris (id, authID, uri, desc, isBackend, needAuth, needLog, keepActive, sort, createTime, updateTime)
VALUES (8, 3, '/api/v1/logout', '前台 [注销登录] 接口', 0, 1, 0, 0, 10102, 63935017487, 63936000000);

INSERT INTO uris (id, authID, uri, desc, isBackend, needAuth, needLog, keepActive, sort, createTime, updateTime)
VALUES (9, 3, '/api/v1/profile', '前台 [获取当前用户信息] 接口', 0, 1, 0, 1, 10103, 63935017488, 63936000000);

INSERT INTO uris (id, authID, uri, desc, isBackend, needAuth, needLog, keepActive, sort, createTime, updateTime)
VALUES (10, 3, '/api/v1/profile/password', '前台 [修改当前用户密码] 接口', 0, 1, 0, 1, 10104, 63935017487, 63936000000);

INSERT INTO uris (id, authID, uri, desc, isBackend, needAuth, needLog, keepActive, sort, createTime, updateTime)
VALUES (11, 3, '/api/v1/balance', '前台 [获取当前用户余额] 接口', 0, 1, 0, 1, 10105, 63935017488, 63936000000);

INSERT INTO uris (id, authID, uri, desc, isBackend, needAuth, needLog, keepActive, sort, createTime, updateTime)
VALUES (12, 3, '/api/v1/balance/log', '前台 [获取余额变动记录] 接口', 0, 1, 0, 1, 10106, 63935017487, 63936000000);

-- ========== 内容模型 - 模型管理 (authID=4) ==========
INSERT INTO uris (id, authID, uri, desc, isBackend, needAuth, needLog, keepActive, sort, createTime, updateTime)
VALUES (13, 4, '/admin/view/model', '[内容模型] 模型列表页面', 1, 1, 0, 1, 1100000, 63935205820, 63936000000);

INSERT INTO uris (id, authID, uri, desc, isBackend, needAuth, needLog, keepActive, sort, createTime, updateTime)
VALUES (14, 4, '/admin/view/model/add', '[内容模型] 添加模型页面', 1, 1, 0, 0, 1100001, 63935205820, 63936000000);

INSERT INTO uris (id, authID, uri, desc, isBackend, needAuth, needLog, keepActive, sort, createTime, updateTime)
VALUES (15, 4, '/admin/view/model/edit', '[内容模型] 编辑模型页面', 1, 1, 0, 0, 1100002, 63935205821, 63936000000);

INSERT INTO uris (id, authID, uri, desc, isBackend, needAuth, needLog, keepActive, sort, createTime, updateTime)
VALUES (16, 4, '/admin/view/model/fields', '[内容模型] 字段管理页面', 1, 1, 0, 0, 1100003, 63935205820, 63936000000);

INSERT INTO uris (id, authID, uri, desc, isBackend, needAuth, needLog, keepActive, sort, createTime, updateTime)
VALUES (17, 4, '/admin/model/list', '[内容模型] 获取模型列表接口', 1, 1, 0, 0, 1100010, 63935205821, 63936000000);

INSERT INTO uris (id, authID, uri, desc, isBackend, needAuth, needLog, keepActive, sort, createTime, updateTime)
VALUES (18, 4, '/admin/model/get', '[内容模型] 获取模型详情接口', 1, 1, 0, 0, 1100011, 63935205820, 63936000000);

INSERT INTO uris (id, authID, uri, desc, isBackend, needAuth, needLog, keepActive, sort, createTime, updateTime)
VALUES (19, 4, '/admin/model/add', '[内容模型] 创建模型接口', 1, 1, 1, 0, 1100012, 63935205820, 63936000000);

INSERT INTO uris (id, authID, uri, desc, isBackend, needAuth, needLog, keepActive, sort, createTime, updateTime)
VALUES (20, 4, '/admin/model/save', '[内容模型] 保存模型接口', 1, 1, 1, 0, 1100013, 63935205820, 63936000000);

INSERT INTO uris (id, authID, uri, desc, isBackend, needAuth, needLog, keepActive, sort, createTime, updateTime)
VALUES (21, 4, '/admin/model/delete', '[内容模型] 删除模型接口', 1, 1, 1, 0, 1100014, 63935205820, 63936000000);

INSERT INTO uris (id, authID, uri, desc, isBackend, needAuth, needLog, keepActive, sort, createTime, updateTime)
VALUES (22, 4, '/admin/model/fields', '[内容模型] 获取模型字段列表接口', 1, 1, 0, 0, 1100020, 63935205821, 63936000000);

INSERT INTO uris (id, authID, uri, desc, isBackend, needAuth, needLog, keepActive, sort, createTime, updateTime)
VALUES (23, 4, '/admin/model/fields/save', '[内容模型] 保存模型字段接口', 1, 1, 1, 0, 1100021, 63935205820, 63936000000);

INSERT INTO uris (id, authID, uri, desc, isBackend, needAuth, needLog, keepActive, sort, createTime, updateTime)
VALUES (24, 4, '/admin/model/compile', '[内容模型] 编译模型接口', 1, 1, 1, 0, 1100030, 63935205820, 63936000000);

INSERT INTO uris (id, authID, uri, desc, isBackend, needAuth, needLog, keepActive, sort, createTime, updateTime)
VALUES (25, 4, '/admin/model/enable', '[内容模型] 启用模型接口', 1, 1, 1, 0, 1100031, 63935205821, 63936000000);

INSERT INTO uris (id, authID, uri, desc, isBackend, needAuth, needLog, keepActive, sort, createTime, updateTime)
VALUES (26, 4, '/admin/model/disable', '[内容模型] 禁用模型接口', 1, 1, 1, 0, 1100032, 63935205820, 63936000000);

-- ========== 附件管理 - 附件列表 (authID=5) ==========
INSERT INTO uris (id, authID, uri, desc, isBackend, needAuth, needLog, keepActive, sort, createTime, updateTime)
VALUES (27, 5, '/admin/view/attachment', '[附件列表] 附件管理页面', 1, 1, 0, 1, 1200000, 63935205821, 63936000000);

INSERT INTO uris (id, authID, uri, desc, isBackend, needAuth, needLog, keepActive, sort, createTime, updateTime)
VALUES (28, 5, '/admin/attachment/list', '[附件列表] 获取附件列表接口', 1, 1, 0, 0, 1200001, 63935205820, 63936000000);

INSERT INTO uris (id, authID, uri, desc, isBackend, needAuth, needLog, keepActive, sort, createTime, updateTime)
VALUES (29, 5, '/admin/attachment/get', '[附件列表] 获取附件详情接口', 1, 1, 0, 0, 1200002, 63935205820, 63936000000);

INSERT INTO uris (id, authID, uri, desc, isBackend, needAuth, needLog, keepActive, sort, createTime, updateTime)
VALUES (30, 5, '/admin/attachment/upload', '[附件列表] 后台上传附件接口', 1, 1, 1, 0, 1200003, 63935205820, 63936000000);

INSERT INTO uris (id, authID, uri, desc, isBackend, needAuth, needLog, keepActive, sort, createTime, updateTime)
VALUES (31, 5, '/admin/attachment/save', '[附件列表] 保存附件信息接口', 1, 1, 1, 0, 1200004, 63935205820, 63936000000);

INSERT INTO uris (id, authID, uri, desc, isBackend, needAuth, needLog, keepActive, sort, createTime, updateTime)
VALUES (32, 5, '/admin/attachment/delete', '[附件列表] 删除附件接口', 1, 1, 1, 0, 1200005, 63935205820, 63936000000);

INSERT INTO uris (id, authID, uri, desc, isBackend, needAuth, needLog, keepActive, sort, createTime, updateTime)
VALUES (33, 5, '/attachment', '[附件访问] 附件下载/预览入口', 1, 0, 0, 0, 1200010, 63935205821, 63936000000);

-- ========== 附件管理 - 存储统计 (authID=6) ==========
INSERT INTO uris (id, authID, uri, desc, isBackend, needAuth, needLog, keepActive, sort, createTime, updateTime)
VALUES (34, 6, '/admin/view/attachment/stats', '[存储统计] 存储统计页面', 1, 1, 0, 1, 1201000, 63935205821, 63936000000);

INSERT INTO uris (id, authID, uri, desc, isBackend, needAuth, needLog, keepActive, sort, createTime, updateTime)
VALUES (35, 6, '/admin/attachment/stats', '[存储统计] 获取存储统计数据接口', 1, 1, 0, 0, 1201001, 63935205821, 63936000000);

-- ========== 附件管理 - 前台附件接口 (authID=7) ==========
INSERT INTO uris (id, authID, uri, desc, isBackend, needAuth, needLog, keepActive, sort, createTime, updateTime)
VALUES (36, 7, '/api/v1/attachment/upload', '前台 [上传附件] 接口', 0, 1, 0, 0, 202000, 63935205821, 63936000000);

INSERT INTO uris (id, authID, uri, desc, isBackend, needAuth, needLog, keepActive, sort, createTime, updateTime)
VALUES (37, 7, '/api/v1/attachment/my', '前台 [我的附件列表] 接口', 0, 1, 0, 0, 202001, 63935205820, 63936000000);

INSERT INTO uris (id, authID, uri, desc, isBackend, needAuth, needLog, keepActive, sort, createTime, updateTime)
VALUES (38, 7, '/api/v1/attachment/purchase', '前台 [购买附件] 接口', 0, 1, 1, 0, 202002, 63935205820, 63936000000);

INSERT INTO uris (id, authID, uri, desc, isBackend, needAuth, needLog, keepActive, sort, createTime, updateTime)
VALUES (39, 7, '/api/v1/attachment/purchased', '前台 [已购附件列表] 接口', 0, 1, 0, 0, 202003, 63935205821, 63936000000);

-- ========== 前台用户管理 - 用户管理 (authID=8) ==========
INSERT INTO uris (id, authID, uri, desc, isBackend, needAuth, needLog, keepActive, sort, createTime, updateTime)
VALUES (40, 8, '/admin/view/member/user', '[用户管理] 用户列表页面', 1, 1, 0, 1, 1300000, 63935017487, 63936000000);

INSERT INTO uris (id, authID, uri, desc, isBackend, needAuth, needLog, keepActive, sort, createTime, updateTime)
VALUES (41, 8, '/admin/view/member/user/add', '[用户管理] 添加用户页面', 1, 1, 0, 0, 1300001, 63935017487, 63936000000);

INSERT INTO uris (id, authID, uri, desc, isBackend, needAuth, needLog, keepActive, sort, createTime, updateTime)
VALUES (42, 8, '/admin/view/member/user/edit', '[用户管理] 编辑用户页面', 1, 1, 0, 0, 1300002, 63935017487, 63936000000);

INSERT INTO uris (id, authID, uri, desc, isBackend, needAuth, needLog, keepActive, sort, createTime, updateTime)
VALUES (43, 8, '/admin/member/user', '[用户管理] 用户CRUD主接口', 1, 1, 1, 1, 1300010, 63935017488, 63936000000);

INSERT INTO uris (id, authID, uri, desc, isBackend, needAuth, needLog, keepActive, sort, createTime, updateTime)
VALUES (44, 8, '/admin/member/user/repwd', '[用户管理] 重置用户密码接口（危险）', 1, 1, 1, 1, 1300011, 63935017488, 63936000000);

INSERT INTO uris (id, authID, uri, desc, isBackend, needAuth, needLog, keepActive, sort, createTime, updateTime)
VALUES (45, 8, '/admin/member/user/balance', '[用户管理] 调整用户余额接口（危险）', 1, 1, 1, 1, 1300012, 63935017488, 63936000000);

-- ========== 前台用户管理 - 用户组管理 (authID=9) ==========
INSERT INTO uris (id, authID, uri, desc, isBackend, needAuth, needLog, keepActive, sort, createTime, updateTime)
VALUES (46, 9, '/admin/view/member/group', '[用户组管理] 用户组列表页面', 1, 1, 0, 1, 1301000, 63935017487, 63936000000);

INSERT INTO uris (id, authID, uri, desc, isBackend, needAuth, needLog, keepActive, sort, createTime, updateTime)
VALUES (47, 9, '/admin/view/member/group/add', '[用户组管理] 添加用户组页面', 1, 1, 0, 0, 1301001, 63935017488, 63936000000);

INSERT INTO uris (id, authID, uri, desc, isBackend, needAuth, needLog, keepActive, sort, createTime, updateTime)
VALUES (48, 9, '/admin/view/member/group/edit', '[用户组管理] 编辑用户组页面', 1, 1, 0, 0, 1301002, 63935017488, 63936000000);

INSERT INTO uris (id, authID, uri, desc, isBackend, needAuth, needLog, keepActive, sort, createTime, updateTime)
VALUES (49, 9, '/admin/member/group', '[用户组管理] 用户组CRUD主接口', 1, 1, 1, 1, 1301010, 63935017488, 63936000000);

-- ========== 前台用户管理 - 权限分类 (authID=10) ==========
INSERT INTO uris (id, authID, uri, desc, isBackend, needAuth, needLog, keepActive, sort, createTime, updateTime)
VALUES (50, 10, '/admin/view/member/authgroup', '[权限分类] 权限分类列表页面', 1, 1, 0, 1, 1302000, 63935017488, 63936000000);

INSERT INTO uris (id, authID, uri, desc, isBackend, needAuth, needLog, keepActive, sort, createTime, updateTime)
VALUES (51, 10, '/admin/view/member/authgroup/add', '[权限分类] 添加分类页面', 1, 1, 0, 0, 1302001, 63935017487, 63936000000);

INSERT INTO uris (id, authID, uri, desc, isBackend, needAuth, needLog, keepActive, sort, createTime, updateTime)
VALUES (52, 10, '/admin/view/member/authgroup/edit', '[权限分类] 编辑分类页面', 1, 1, 0, 0, 1302002, 63935017488, 63936000000);

INSERT INTO uris (id, authID, uri, desc, isBackend, needAuth, needLog, keepActive, sort, createTime, updateTime)
VALUES (53, 10, '/admin/member/authgroup', '[权限分类] 权限分类CRUD主接口', 1, 1, 1, 1, 1302010, 63935017488, 63936000000);

-- ========== 前台用户管理 - 权限分组 (authID=11) ==========
INSERT INTO uris (id, authID, uri, desc, isBackend, needAuth, needLog, keepActive, sort, createTime, updateTime)
VALUES (54, 11, '/admin/view/member/auth', '[权限分组] 权限分组列表页面', 1, 1, 0, 1, 1303000, 63935017487, 63936000000);

INSERT INTO uris (id, authID, uri, desc, isBackend, needAuth, needLog, keepActive, sort, createTime, updateTime)
VALUES (55, 11, '/admin/view/member/auth/add', '[权限分组] 添加分组页面', 1, 1, 0, 0, 1303001, 63935017487, 63936000000);

INSERT INTO uris (id, authID, uri, desc, isBackend, needAuth, needLog, keepActive, sort, createTime, updateTime)
VALUES (56, 11, '/admin/view/member/auth/edit', '[权限分组] 编辑分组页面', 1, 1, 0, 0, 1303002, 63935017487, 63936000000);

INSERT INTO uris (id, authID, uri, desc, isBackend, needAuth, needLog, keepActive, sort, createTime, updateTime)
VALUES (57, 11, '/admin/member/auth', '[权限分组] 权限分组CRUD主接口', 1, 1, 1, 1, 1303010, 63935017488, 63936000000);

-- ========== 后台权限管理 - 用户管理 (authID=12) ==========
INSERT INTO uris (id, authID, uri, desc, isBackend, needAuth, needLog, keepActive, sort, createTime, updateTime)
VALUES (58, 12, '/admin/view/auth/user', '[用户管理] 后台用户列表页面', 1, 1, 0, 1, 1400000, 63933638061, 63936000000);

INSERT INTO uris (id, authID, uri, desc, isBackend, needAuth, needLog, keepActive, sort, createTime, updateTime)
VALUES (59, 12, '/admin/view/auth/user/add', '[用户管理] 添加后台用户页面', 1, 1, 0, 0, 1400001, 63933814573, 63936000000);

INSERT INTO uris (id, authID, uri, desc, isBackend, needAuth, needLog, keepActive, sort, createTime, updateTime)
VALUES (60, 12, '/admin/view/auth/user/edit', '[用户管理] 编辑后台用户页面', 1, 1, 0, 0, 1400002, 63933814573, 63936000000);

INSERT INTO uris (id, authID, uri, desc, isBackend, needAuth, needLog, keepActive, sort, createTime, updateTime)
VALUES (61, 12, '/admin/auth/user', '[用户管理] 后台用户CRUD主接口', 1, 1, 1, 1, 1400010, 63933638061, 63936000000);

INSERT INTO uris (id, authID, uri, desc, isBackend, needAuth, needLog, keepActive, sort, createTime, updateTime)
VALUES (62, 12, '/admin/auth/user/repwd', '[用户管理] 重置后台用户密码接口（危险）', 1, 1, 1, 1, 1400011, 63933638061, 63936000000);

-- ========== 后台权限管理 - 角色管理 (authID=13) ==========
INSERT INTO uris (id, authID, uri, desc, isBackend, needAuth, needLog, keepActive, sort, createTime, updateTime)
VALUES (63, 13, '/admin/view/auth/role', '[角色管理] 角色列表页面', 1, 1, 0, 1, 1401000, 63933638061, 63936000000);

INSERT INTO uris (id, authID, uri, desc, isBackend, needAuth, needLog, keepActive, sort, createTime, updateTime)
VALUES (64, 13, '/admin/view/auth/role/add', '[角色管理] 添加角色页面', 1, 1, 0, 0, 1401001, 63933809706, 63936000000);

INSERT INTO uris (id, authID, uri, desc, isBackend, needAuth, needLog, keepActive, sort, createTime, updateTime)
VALUES (65, 13, '/admin/view/auth/role/edit', '[角色管理] 编辑角色页面', 1, 1, 0, 0, 1401002, 63933809706, 63936000000);

INSERT INTO uris (id, authID, uri, desc, isBackend, needAuth, needLog, keepActive, sort, createTime, updateTime)
VALUES (66, 13, '/admin/auth/role', '[角色管理] 角色CRUD主接口', 1, 1, 1, 1, 1401010, 63933638061, 63936000000);

-- ========== 后台权限管理 - 权限分类 (authID=14) ==========
INSERT INTO uris (id, authID, uri, desc, isBackend, needAuth, needLog, keepActive, sort, createTime, updateTime)
VALUES (67, 14, '/admin/view/auth/group', '[权限分类] 权限分类列表页面', 1, 1, 0, 1, 1402000, 63933638061, 63936000000);

INSERT INTO uris (id, authID, uri, desc, isBackend, needAuth, needLog, keepActive, sort, createTime, updateTime)
VALUES (68, 14, '/admin/view/auth/group/add', '[权限分类] 添加分类页面', 1, 1, 0, 0, 1402001, 63933805653, 63936000000);

INSERT INTO uris (id, authID, uri, desc, isBackend, needAuth, needLog, keepActive, sort, createTime, updateTime)
VALUES (69, 14, '/admin/view/auth/group/edit', '[权限分类] 编辑分类页面', 1, 1, 0, 0, 1402002, 63933805653, 63936000000);

INSERT INTO uris (id, authID, uri, desc, isBackend, needAuth, needLog, keepActive, sort, createTime, updateTime)
VALUES (70, 14, '/admin/auth/group', '[权限分类] 权限分类CRUD主接口', 1, 1, 1, 1, 1402010, 63933638061, 63936000000);

-- ========== 后台权限管理 - 权限分组 (authID=15) ==========
INSERT INTO uris (id, authID, uri, desc, isBackend, needAuth, needLog, keepActive, sort, createTime, updateTime)
VALUES (71, 15, '/admin/view/auth/auth', '[权限分组] 权限分组列表页面', 1, 1, 0, 1, 1403000, 63933638060, 63936000000);

INSERT INTO uris (id, authID, uri, desc, isBackend, needAuth, needLog, keepActive, sort, createTime, updateTime)
VALUES (72, 15, '/admin/view/auth/auth/add', '[权限分组] 添加分组页面', 1, 1, 0, 0, 1403001, 63933720099, 63936000000);

INSERT INTO uris (id, authID, uri, desc, isBackend, needAuth, needLog, keepActive, sort, createTime, updateTime)
VALUES (73, 15, '/admin/view/auth/auth/edit', '[权限分组] 编辑分组页面', 1, 1, 0, 0, 1403002, 63933720099, 63936000000);

INSERT INTO uris (id, authID, uri, desc, isBackend, needAuth, needLog, keepActive, sort, createTime, updateTime)
VALUES (74, 15, '/admin/auth/auth', '[权限分组] 权限分组CRUD主接口', 1, 1, 1, 1, 1403010, 63933638061, 63936000000);

-- ========== 设置 - 设置管理 (authID=16) ==========
INSERT INTO uris (id, authID, uri, desc, isBackend, needAuth, needLog, keepActive, sort, createTime, updateTime)
VALUES (75, 16, '/admin/view/option', '[设置管理] 配置页面视图', 1, 1, 0, 1, 1500000, 63934828369, 63936000000);

INSERT INTO uris (id, authID, uri, desc, isBackend, needAuth, needLog, keepActive, sort, createTime, updateTime)
VALUES (76, 16, '/admin/option', '[设置管理] 配置读写主接口', 1, 1, 1, 1, 1500001, 63934828369, 63936000000);

-- ========== 设置 - 菜单管理 (authID=17) ==========
INSERT INTO uris (id, authID, uri, desc, isBackend, needAuth, needLog, keepActive, sort, createTime, updateTime)
VALUES (77, 17, '/admin/view/option/menu', '[菜单管理] 菜单列表页面', 1, 1, 0, 1, 1501000, 63934829866, 63936000000);

INSERT INTO uris (id, authID, uri, desc, isBackend, needAuth, needLog, keepActive, sort, createTime, updateTime)
VALUES (78, 17, '/admin/view/option/menu/add', '[菜单管理] 添加菜单页面', 1, 1, 0, 0, 1501001, 63934829866, 63936000000);

INSERT INTO uris (id, authID, uri, desc, isBackend, needAuth, needLog, keepActive, sort, createTime, updateTime)
VALUES (79, 17, '/admin/view/option/menu/add/category', '[菜单管理] 添加分类页面', 1, 1, 0, 0, 1501002, 63934843214, 63936000000);

INSERT INTO uris (id, authID, uri, desc, isBackend, needAuth, needLog, keepActive, sort, createTime, updateTime)
VALUES (80, 17, '/admin/view/option/menu/edit', '[菜单管理] 编辑菜单页面', 1, 1, 0, 0, 1501003, 63934829866, 63936000000);

INSERT INTO uris (id, authID, uri, desc, isBackend, needAuth, needLog, keepActive, sort, createTime, updateTime)
VALUES (81, 17, '/admin/option/menu', '[菜单管理] 菜单CRUD主接口', 1, 1, 1, 1, 1501010, 63934829866, 63936000000);

-- ========== 设置 - 接口管理 (authID=18) ==========
INSERT INTO uris (id, authID, uri, desc, isBackend, needAuth, needLog, keepActive, sort, createTime, updateTime)
VALUES (82, 18, '/admin/view/auth/uris', '[接口管理] 接口列表页面', 1, 1, 0, 1, 1502000, 63933638060, 63936000000);

INSERT INTO uris (id, authID, uri, desc, isBackend, needAuth, needLog, keepActive, sort, createTime, updateTime)
VALUES (83, 18, '/admin/view/auth/uris/edit', '[接口管理] 编辑接口页面', 1, 1, 0, 0, 1502001, 63933638061, 63936000000);

INSERT INTO uris (id, authID, uri, desc, isBackend, needAuth, needLog, keepActive, sort, createTime, updateTime)
VALUES (84, 18, '/admin/auth/uris', '[接口管理] 接口CRUD主接口', 1, 1, 1, 1, 1502010, 63933638061, 63936000000);

-- ========== 系统日志 (authID=19) ==========
INSERT INTO uris (id, authID, uri, desc, isBackend, needAuth, needLog, keepActive, sort, createTime, updateTime)
VALUES (85, 19, '/admin/view/logs', '[系统日志] 日志列表页面', 1, 1, 0, 1, 1600000, 63933640875, 63936000000);

INSERT INTO uris (id, authID, uri, desc, isBackend, needAuth, needLog, keepActive, sort, createTime, updateTime)
VALUES (86, 19, '/admin/logs', '[系统日志] 获取日志列表接口', 1, 1, 0, 1, 1600001, 63933638061, 63936000000);

INSERT INTO uris (id, authID, uri, desc, isBackend, needAuth, needLog, keepActive, sort, createTime, updateTime)
VALUES (87, 19, '/admin/logs/clear', '[系统日志] 清除7天前日志接口', 1, 1, 1, 1, 1600002, 63933638061, 63936000000);

-- ========== 调试接口 (authID=20) ==========
INSERT INTO uris (id, authID, uri, desc, isBackend, needAuth, needLog, keepActive, sort, createTime, updateTime)
VALUES (88, 20, '/admin/trace', '[调试] 获取全局缓存概览', 1, 1, 0, 1, 1700000, 63934856182, 63936000000);

INSERT INTO uris (id, authID, uri, desc, isBackend, needAuth, needLog, keepActive, sort, createTime, updateTime)
VALUES (89, 20, '/admin/trace/session', '[调试] 获取完整Session缓存数据', 1, 1, 0, 1, 1700001, 63934856181, 63936000000);

INSERT INTO uris (id, authID, uri, desc, isBackend, needAuth, needLog, keepActive, sort, createTime, updateTime)
VALUES (90, 20, '/admin/trace/option', '[调试] 获取完整Option配置缓存', 1, 1, 0, 1, 1700002, 63934856182, 63936000000);

INSERT INTO uris (id, authID, uri, desc, isBackend, needAuth, needLog, keepActive, sort, createTime, updateTime)
VALUES (91, 20, '/admin/trace/auth', '[调试] 获取权限相关缓存数据', 1, 1, 0, 1, 1700003, 63934856181, 63936000000);

INSERT INTO uris (id, authID, uri, desc, isBackend, needAuth, needLog, keepActive, sort, createTime, updateTime)
VALUES (92, 20, '/admin/trace/route', '[调试] 获取路由表数据', 1, 1, 0, 1, 1700004, 63934856181, 63936000000);

-- ========== 前台评论接口 (归入未分组 authID=1，动态模型使用) ==========
INSERT INTO uris (id, authID, uri, desc, isBackend, needAuth, needLog, keepActive, sort, createTime, updateTime)
VALUES (93, 1, '/api/v1/cms/article/reply/add', '前台 [文章评论] 添加评论接口', 0, 1, 0, 0, 900000, 63935205821, 63936000000);

INSERT INTO uris (id, authID, uri, desc, isBackend, needAuth, needLog, keepActive, sort, createTime, updateTime)
VALUES (94, 1, '/api/v1/cms/article/reply/delete', '前台 [文章评论] 删除评论接口', 0, 1, 1, 0, 900001, 63935205820, 63936000000);


-- ============================================================
-- 索引定义
-- ============================================================

-- 索引：idx_attachment_ext
CREATE INDEX idx_attachment_ext ON attachment (ext);

-- 索引：idx_attachment_model
CREATE INDEX idx_attachment_model ON attachment (modelName, recordId);

-- 索引：idx_attachment_time
CREATE INDEX idx_attachment_time ON attachment (createTime);

-- 索引：idx_attachment_uploader
CREATE INDEX idx_attachment_uploader ON attachment (uploaderId, uploaderType);

-- 索引：idx_auth_group
CREATE INDEX idx_auth_group ON auth (groupID);

-- 索引：idx_balance_member
CREATE INDEX idx_balance_member ON memberBalanceLog (memberId);

-- 索引：idx_balance_time
CREATE INDEX idx_balance_time ON memberBalanceLog (createTime);

-- 索引：idx_logs_time
CREATE INDEX idx_logs_time ON logs (createTime);

-- 索引：idx_logs_uri
CREATE INDEX idx_logs_uri ON logs (uri);

-- 索引：idx_logs_user
CREATE INDEX idx_logs_user ON logs (user);

-- 索引：idx_member_email
CREATE UNIQUE INDEX idx_member_email ON member (email)
WHERE email IS NOT NULL AND email != '';

-- 索引：idx_member_group
CREATE INDEX idx_member_group ON member (groupId);

-- 索引：idx_member_phone
CREATE UNIQUE INDEX idx_member_phone ON member (phone)
WHERE phone IS NOT NULL AND phone != '';

-- 索引：idx_member_status
CREATE INDEX idx_member_status ON member (status, isDelete);

-- 索引：idx_memberauth_group
CREATE INDEX idx_memberauth_group ON memberAuth (groupID);

-- 索引：idx_menu_parent
CREATE INDEX idx_menu_parent ON menu (parent);

-- 索引：idx_order_member
CREATE INDEX idx_order_member ON attachmentOrder (memberId);

-- 索引：idx_order_seller
CREATE INDEX idx_order_seller ON attachmentOrder (sellerId);

-- 索引：idx_order_time
CREATE INDEX idx_order_time ON attachmentOrder (createTime);

-- 索引：idx_order_unique
CREATE UNIQUE INDEX idx_order_unique ON attachmentOrder (attachmentXid, memberId);

-- 索引：idx_order_xid
CREATE INDEX idx_order_xid ON attachmentOrder (attachmentXid);

-- 索引：idx_reply_content
CREATE INDEX idx_reply_content ON reply (modelName, contentId);

-- 索引：idx_reply_time
CREATE INDEX idx_reply_time ON reply (createTime);

-- 索引：idx_reply_user
CREATE INDEX idx_reply_user ON reply (userId);

-- 索引：idx_uris_auth
CREATE INDEX idx_uris_auth ON uris (authID);


COMMIT TRANSACTION;
PRAGMA foreign_keys = on;
