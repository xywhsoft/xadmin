--
-- SQLiteStudio v3.4.17 生成的文件，周二 1月 6 20:21:12 2026
--
-- 所用的文本编码：UTF-8
--
PRAGMA foreign_keys = off;
BEGIN TRANSACTION;


-- 表：logs
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


-- 表：menu
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

INSERT INTO menu (
                     id,
                     parent,
                     title,
                     icon,
                     type,
                     openType,
                     href,
                     sort,
                     visible,
                     remark,
                     createTime,
                     updateTime,
                     isDelete
                 )
                 VALUES (
                     1,
                     0,
                     '主页',
                     'layui-icon layui-icon-home',
                     1,
                     '_iframe',
                     'view/home',
                     0,
                     1,
                     '系统主页',
                     63933640350,
                     63933640350,
                     0
                 );

INSERT INTO menu (
                     id,
                     parent,
                     title,
                     icon,
                     type,
                     openType,
                     href,
                     sort,
                     visible,
                     remark,
                     createTime,
                     updateTime,
                     isDelete
                 )
                 VALUES (
                     2,
                     0,
                     '系统日志',
                     'layui-icon layui-icon-log',
                     1,
                     '_component',
                     'view/logs',
                     100000,
                     1,
                     '查看系统日志',
                     63933640350,
                     63933640350,
                     0
                 );

INSERT INTO menu (
                     id,
                     parent,
                     title,
                     icon,
                     type,
                     openType,
                     href,
                     sort,
                     visible,
                     remark,
                     createTime,
                     updateTime,
                     isDelete
                 )
                 VALUES (
                     3,
                     0,
                     '后台权限管理',
                     'layui-icon layui-icon-auz',
                     0,
                     '',
                     '',
                     400000,
                     1,
                     '权限管理目录',
                     63933640350,
                     63934948566,
                     0
                 );

INSERT INTO menu (
                     id,
                     parent,
                     title,
                     icon,
                     type,
                     openType,
                     href,
                     sort,
                     visible,
                     remark,
                     createTime,
                     updateTime,
                     isDelete
                 )
                 VALUES (
                     4,
                     3,
                     '用户管理',
                     'layui-icon layui-icon-username',
                     1,
                     '_component',
                     'view/auth/user',
                     400100,
                     1,
                     '管理后台用户',
                     63933640350,
                     63934948571,
                     0
                 );

INSERT INTO menu (
                     id,
                     parent,
                     title,
                     icon,
                     type,
                     openType,
                     href,
                     sort,
                     visible,
                     remark,
                     createTime,
                     updateTime,
                     isDelete
                 )
                 VALUES (
                     5,
                     3,
                     '角色管理',
                     'layui-icon layui-icon-user',
                     1,
                     '_component',
                     'view/auth/role',
                     400200,
                     1,
                     '管理用户角色',
                     63933640350,
                     63934948585,
                     0
                 );

INSERT INTO menu (
                     id,
                     parent,
                     title,
                     icon,
                     type,
                     openType,
                     href,
                     sort,
                     visible,
                     remark,
                     createTime,
                     updateTime,
                     isDelete
                 )
                 VALUES (
                     6,
                     3,
                     '权限分类',
                     'layui-icon layui-icon-tabs',
                     1,
                     '_component',
                     'view/auth/group',
                     400300,
                     1,
                     '管理权限分类',
                     63933640350,
                     63934948581,
                     0
                 );

INSERT INTO menu (
                     id,
                     parent,
                     title,
                     icon,
                     type,
                     openType,
                     href,
                     sort,
                     visible,
                     remark,
                     createTime,
                     updateTime,
                     isDelete
                 )
                 VALUES (
                     7,
                     3,
                     '权限分组',
                     'layui-icon layui-icon-template',
                     1,
                     '_component',
                     'view/auth/auth',
                     400400,
                     1,
                     '管理权限分组',
                     63933640350,
                     63934948578,
                     0
                 );

INSERT INTO menu (
                     id,
                     parent,
                     title,
                     icon,
                     type,
                     openType,
                     href,
                     sort,
                     visible,
                     remark,
                     createTime,
                     updateTime,
                     isDelete
                 )
                 VALUES (
                     8,
                     3,
                     '接口管理',
                     'layui-icon layui-icon-website',
                     1,
                     '_component',
                     'view/auth/uris',
                     400500,
                     1,
                     '管理API接口',
                     63933640350,
                     63934948575,
                     0
                 );

INSERT INTO menu (
                     id,
                     parent,
                     title,
                     icon,
                     type,
                     openType,
                     href,
                     sort,
                     visible,
                     remark,
                     createTime,
                     updateTime,
                     isDelete
                 )
                 VALUES (
                     9,
                     11,
                     '菜单管理',
                     'layui-icon layui-icon-spread-left',
                     1,
                     '_component',
                     'view/option/menu',
                     203000,
                     1,
                     '管理系统菜单',
                     63933640350,
                     63934948612,
                     0
                 );

INSERT INTO menu (
                     id,
                     parent,
                     title,
                     icon,
                     type,
                     openType,
                     href,
                     sort,
                     visible,
                     remark,
                     createTime,
                     updateTime,
                     isDelete
                 )
                 VALUES (
                     10,
                     11,
                     '全局配置',
                     'layui-icon layui-icon-set',
                     1,
                     '_iframe',
                     'view/option?file=global.json',
                     201000,
                     1,
                     '系统全局配置管理',
                     63933640350,
                     63934948596,
                     0
                 );

INSERT INTO menu (
                     id,
                     parent,
                     title,
                     icon,
                     type,
                     openType,
                     href,
                     sort,
                     visible,
                     remark,
                     createTime,
                     updateTime,
                     isDelete
                 )
                 VALUES (
                     11,
                     0,
                     '设置',
                     'layui-icon layui-icon-set',
                     0,
                     '',
                     '',
                     200000,
                     1,
                     '设置目录',
                     63934847322,
                     63934848303,
                     0
                 );

INSERT INTO menu (
                     id,
                     parent,
                     title,
                     icon,
                     type,
                     openType,
                     href,
                     sort,
                     visible,
                     remark,
                     createTime,
                     updateTime,
                     isDelete
                 )
                 VALUES (
                     13,
                     11,
                     '自定义设置',
                     'layui-icon layui-icon-face-smile',
                     1,
                     '_iframe',
                     'view/option?file=my.json',
                     202000,
                     1,
                     '',
                     63934855728,
                     63934948603,
                     0
                 );

INSERT INTO menu (
                     id,
                     parent,
                     title,
                     icon,
                     type,
                     openType,
                     href,
                     sort,
                     visible,
                     remark,
                     createTime,
                     updateTime,
                     isDelete
                 )
                 VALUES (
                     14,
                     0,
                     '前台权限管理',
                     'layui-icon layui-icon-user',
                     0,
                     '',
                     '',
                     300000,
                     1,
                     '',
                     63934948636,
                     63934949797,
                     0
                 );

INSERT INTO menu (
                     id,
                     parent,
                     title,
                     icon,
                     type,
                     openType,
                     href,
                     sort,
                     visible,
                     remark,
                     createTime,
                     updateTime,
                     isDelete
                 )
                 VALUES (
                     15,
                     14,
                     '用户管理',
                     'layui-icon layui-icon-username',
                     1,
                     '_component',
                     '',
                     301000,
                     1,
                     '',
                     63934949844,
                     63934949856,
                     0
                 );


-- 表：user
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

INSERT INTO user (
                     id,
                     user,
                     salt,
                     pwd,
                     role,
                     authLevel,
                     createTime,
                     updateTime,
                     isDelete
                 )
                 VALUES (
                     1,
                     'admin',
                     'yT30uWu00001a1EiofyV4b9-_aHNO0aS',
                     '4FD681049123E6D9E48D534D7AA7DACEDE38C8860B14069647FF1E4446573521',
                     1,
                     0,
                     63933640350,
                     63933829361,
                     0
                 );


-- 表：role
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

INSERT INTO role (
                     id,
                     name,
                     desc,
                     authList,
                     authLevel,
                     createTime,
                     updateTime,
                     isDelete
                 )
                 VALUES (
                     1,
                     '超级管理员',
                     '超级管理员账户，拥有后台的全部权限',
                     '[1,2,3,4,5,6,7,8,9,10,11,12,13,14,15]',
                     999,
                     63930031353,
                     63934938596,
                     0
                 );

INSERT INTO role (
                     id,
                     name,
                     desc,
                     authList,
                     authLevel,
                     createTime,
                     updateTime,
                     isDelete
                 )
                 VALUES (
                     2,
                     '管理员',
                     '拥有除了权限管理外的所有权限',
                     '[2,3,4,11,13,14]',
                     200,
                     63930031443,
                     63934938603,
                     0
                 );


-- 表：authGroup
CREATE TABLE authGroup (
    id         INTEGER PRIMARY KEY AUTOINCREMENT,
    name       TEXT,
    desc       TEXT,
    sort       INTEGER,
    createTime INTEGER,
    updateTime INTEGER,
    isDelete   INTEGER DEFAULT (0) 
);

INSERT INTO authGroup (
                          id,
                          name,
                          desc,
                          sort,
                          createTime,
                          updateTime,
                          isDelete
                      )
                      VALUES (
                          1,
                          '未分类',
                          '未分类的权限',
-                         1,
                          63930028060,
                          63930029645,
                          0
                      );

INSERT INTO authGroup (
                          id,
                          name,
                          desc,
                          sort,
                          createTime,
                          updateTime,
                          isDelete
                      )
                      VALUES (
                          2,
                          '服务管理',
                          '[服务管理] 菜单',
                          1000,
                          63933807197,
                          63933903128,
                          1
                      );

INSERT INTO authGroup (
                          id,
                          name,
                          desc,
                          sort,
                          createTime,
                          updateTime,
                          isDelete
                      )
                      VALUES (
                          3,
                          '系统日志',
                          '[系统日志] 菜单',
                          100000,
                          63933903025,
                          63933903063,
                          0
                      );

INSERT INTO authGroup (
                          id,
                          name,
                          desc,
                          sort,
                          createTime,
                          updateTime,
                          isDelete
                      )
                      VALUES (
                          4,
                          '后台权限管理',
                          '[后台权限管理] 菜单',
                          400000,
                          63933903052,
                          63934948278,
                          0
                      );

INSERT INTO authGroup (
                          id,
                          name,
                          desc,
                          sort,
                          createTime,
                          updateTime,
                          isDelete
                      )
                      VALUES (
                          5,
                          '基础权限',
                          '主页、登录等页面',
                          0,
                          63933903097,
                          63933903097,
                          0
                      );

INSERT INTO authGroup (
                          id,
                          name,
                          desc,
                          sort,
                          createTime,
                          updateTime,
                          isDelete
                      )
                      VALUES (
                          6,
                          '服务列表',
                          '[服务列表] 菜单',
                          2000,
                          63933903142,
                          63933903142,
                          1
                      );

INSERT INTO authGroup (
                          id,
                          name,
                          desc,
                          sort,
                          createTime,
                          updateTime,
                          isDelete
                      )
                      VALUES (
                          7,
                          '设置',
                          '[设置] 菜单',
                          200000,
                          63934948202,
                          63934948202,
                          0
                      );

INSERT INTO authGroup (
                          id,
                          name,
                          desc,
                          sort,
                          createTime,
                          updateTime,
                          isDelete
                      )
                      VALUES (
                          8,
                          '前台用户管理',
                          '[前台用户管理] 菜单',
                          300000,
                          63934948299,
                          63934948299,
                          0
                      );

INSERT INTO authGroup (
                          id,
                          name,
                          desc,
                          sort,
                          createTime,
                          updateTime,
                          isDelete
                      )
                      VALUES (
                          9,
                          '调试接口',
                          '用于调试的接口',
                          500000,
                          63934949629,
                          63934949629,
                          0
                      );


-- 表：auth
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

INSERT INTO auth (
                     id,
                     groupID,
                     name,
                     desc,
                     sort,
                     createTime,
                     updateTime,
                     isDelete
                 )
                 VALUES (
                     1,
                     1,
                     '未分组',
                     '未经分组的 URI 会被归类到这个分类下',
-                    1,
                     63930019316,
                     63933806362,
                     0
                 );

INSERT INTO auth (
                     id,
                     groupID,
                     name,
                     desc,
                     sort,
                     createTime,
                     updateTime,
                     isDelete
                 )
                 VALUES (
                     2,
                     1,
                     '服务管理',
                     '[服务管理] 页面和接口',
                     1000,
                     63933802678,
                     63933903248,
                     0
                 );

INSERT INTO auth (
                     id,
                     groupID,
                     name,
                     desc,
                     sort,
                     createTime,
                     updateTime,
                     isDelete
                 )
                 VALUES (
                     3,
                     5,
                     '基础页面',
                     '主页、登录页面等非功能归类页面',
                     0,
                     63933903210,
                     63933903210,
                     0
                 );

INSERT INTO auth (
                     id,
                     groupID,
                     name,
                     desc,
                     sort,
                     createTime,
                     updateTime,
                     isDelete
                 )
                 VALUES (
                     4,
                     1,
                     '服务列表',
                     '[任务管理] 页面和接口',
                     2000,
                     63933903275,
                     63933903275,
                     0
                 );

INSERT INTO auth (
                     id,
                     groupID,
                     name,
                     desc,
                     sort,
                     createTime,
                     updateTime,
                     isDelete
                 )
                 VALUES (
                     5,
                     3,
                     '系统日志',
                     '[系统日志] 页面和接口',
                     100000,
                     63933903308,
                     63933903308,
                     0
                 );

INSERT INTO auth (
                     id,
                     groupID,
                     name,
                     desc,
                     sort,
                     createTime,
                     updateTime,
                     isDelete
                 )
                 VALUES (
                     6,
                     4,
                     '用户管理',
                     '[权限管理] - [用户管理] 页面和接口',
                     400000,
                     63933903342,
                     63934948319,
                     0
                 );

INSERT INTO auth (
                     id,
                     groupID,
                     name,
                     desc,
                     sort,
                     createTime,
                     updateTime,
                     isDelete
                 )
                 VALUES (
                     7,
                     4,
                     '角色管理',
                     '[权限管理] - [角色管理] 页面和接口',
                     401000,
                     63933903366,
                     63934948316,
                     0
                 );

INSERT INTO auth (
                     id,
                     groupID,
                     name,
                     desc,
                     sort,
                     createTime,
                     updateTime,
                     isDelete
                 )
                 VALUES (
                     8,
                     4,
                     '权限分类',
                     '[权限管理] - [权限分类] 页面和接口',
                     402000,
                     63933903387,
                     63934948313,
                     0
                 );

INSERT INTO auth (
                     id,
                     groupID,
                     name,
                     desc,
                     sort,
                     createTime,
                     updateTime,
                     isDelete
                 )
                 VALUES (
                     9,
                     4,
                     '权限分组',
                     '[权限管理] - [权限分组] 页面和接口',
                     403000,
                     63933903428,
                     63934948310,
                     0
                 );

INSERT INTO auth (
                     id,
                     groupID,
                     name,
                     desc,
                     sort,
                     createTime,
                     updateTime,
                     isDelete
                 )
                 VALUES (
                     10,
                     4,
                     '接口管理',
                     '[权限管理] - [接口管理] 页面和接口',
                     404000,
                     63933903456,
                     63934948308,
                     0
                 );

INSERT INTO auth (
                     id,
                     groupID,
                     name,
                     desc,
                     sort,
                     createTime,
                     updateTime,
                     isDelete
                 )
                 VALUES (
                     11,
                     7,
                     '设置管理',
                     '所有设置页面和接口',
                     200000,
                     63934948478,
                     63934948478,
                     0
                 );

INSERT INTO auth (
                     id,
                     groupID,
                     name,
                     desc,
                     sort,
                     createTime,
                     updateTime,
                     isDelete
                 )
                 VALUES (
                     12,
                     7,
                     '菜单管理',
                     '[设置] - [菜单管理] 页面和接口',
                     201000,
                     63934948510,
                     63934948510,
                     0
                 );

INSERT INTO auth (
                     id,
                     groupID,
                     name,
                     desc,
                     sort,
                     createTime,
                     updateTime,
                     isDelete
                 )
                 VALUES (
                     13,
                     8,
                     '用户管理',
                     '[前台用户管理] - [用户管理] 页面和接口',
                     300000,
                     63934949282,
                     63934949282,
                     0
                 );

INSERT INTO auth (
                     id,
                     groupID,
                     name,
                     desc,
                     sort,
                     createTime,
                     updateTime,
                     isDelete
                 )
                 VALUES (
                     14,
                     8,
                     '用户组管理',
                     '[前台用户管理] - [用户组管理] 页面和接口',
                     301000,
                     63934949319,
                     63934949325,
                     0
                 );

INSERT INTO auth (
                     id,
                     groupID,
                     name,
                     desc,
                     sort,
                     createTime,
                     updateTime,
                     isDelete
                 )
                 VALUES (
                     15,
                     9,
                     '调试接口',
                     '用于调试的接口',
                     500000,
                     63934949645,
                     63934949649,
                     0
                 );


-- 表：uris
CREATE TABLE uris (
    id         INTEGER PRIMARY KEY AUTOINCREMENT
                       UNIQUE,
    authID     INTEGER REFERENCES auth (id),
    uri        TEXT    UNIQUE,
    desc       TEXT,
    sort       INTEGER,
    createTime INTEGER,
    updateTime INTEGER
);

INSERT INTO uris (
                     id,
                     authID,
                     uri,
                     desc,
                     sort,
                     createTime,
                     updateTime
                 )
                 VALUES (
                     1,
                     10,
                     '/view/auth/uris',
                     '[接口管理] 主页面视图',
                     404000,
                     63933638060,
                     63933904195
                 );

INSERT INTO uris (
                     id,
                     authID,
                     uri,
                     desc,
                     sort,
                     createTime,
                     updateTime
                 )
                 VALUES (
                     2,
                     9,
                     '/view/auth/auth',
                     '[权限分组] 主页面视图',
                     403000,
                     63933638060,
                     63933904188
                 );

INSERT INTO uris (
                     id,
                     authID,
                     uri,
                     desc,
                     sort,
                     createTime,
                     updateTime
                 )
                 VALUES (
                     3,
                     6,
                     '/view/auth/user',
                     '[用户管理] 主页面视图',
                     400000,
                     63933638061,
                     63933904184
                 );

INSERT INTO uris (
                     id,
                     authID,
                     uri,
                     desc,
                     sort,
                     createTime,
                     updateTime
                 )
                 VALUES (
                     4,
                     8,
                     '/view/auth/group',
                     '[权限分类] 主页面视图',
                     402000,
                     63933638061,
                     63933904174
                 );

INSERT INTO uris (
                     id,
                     authID,
                     uri,
                     desc,
                     sort,
                     createTime,
                     updateTime
                 )
                 VALUES (
                     5,
                     10,
                     '/auth/uris',
                     '[接口管理] 主接口',
                     404010,
                     63933638061,
                     63933904305
                 );

INSERT INTO uris (
                     id,
                     authID,
                     uri,
                     desc,
                     sort,
                     createTime,
                     updateTime
                 )
                 VALUES (
                     6,
                     8,
                     '/auth/group',
                     '[权限分类] 主接口',
                     402010,
                     63933638061,
                     63933904326
                 );

INSERT INTO uris (
                     id,
                     authID,
                     uri,
                     desc,
                     sort,
                     createTime,
                     updateTime
                 )
                 VALUES (
                     7,
                     3,
                     '/view/home',
                     '管理后台主页',
                     10,
                     63933638061,
                     63933904962
                 );

INSERT INTO uris (
                     id,
                     authID,
                     uri,
                     desc,
                     sort,
                     createTime,
                     updateTime
                 )
                 VALUES (
                     8,
                     10,
                     '/view/auth/uris/edit',
                     '[接口管理] - [编辑接口] 子页面视图',
                     404020,
                     63933638061,
                     63934948331
                 );

INSERT INTO uris (
                     id,
                     authID,
                     uri,
                     desc,
                     sort,
                     createTime,
                     updateTime
                 )
                 VALUES (
                     9,
                     7,
                     '/view/auth/role',
                     '[角色管理] 主页面视图',
                     401000,
                     63933638061,
                     63933904214
                 );

INSERT INTO uris (
                     id,
                     authID,
                     uri,
                     desc,
                     sort,
                     createTime,
                     updateTime
                 )
                 VALUES (
                     10,
                     6,
                     '/auth/user/repwd',
                     '[用户管理] 无条件重置密码接口（危险权限）',
                     400040,
                     63933638061,
                     63933904541
                 );

INSERT INTO uris (
                     id,
                     authID,
                     uri,
                     desc,
                     sort,
                     createTime,
                     updateTime
                 )
                 VALUES (
                     11,
                     3,
                     '/',
                     '管理后台',
                     0,
                     63933638061,
                     63933904949
                 );

INSERT INTO uris (
                     id,
                     authID,
                     uri,
                     desc,
                     sort,
                     createTime,
                     updateTime
                 )
                 VALUES (
                     12,
                     5,
                     '/logs',
                     '[系统日志] 主接口',
                     100010,
                     63933638061,
                     63933904869
                 );

INSERT INTO uris (
                     id,
                     authID,
                     uri,
                     desc,
                     sort,
                     createTime,
                     updateTime
                 )
                 VALUES (
                     13,
                     7,
                     '/auth/role',
                     '[角色管理] 主接口',
                     401010,
                     63933638061,
                     63933904366
                 );

INSERT INTO uris (
                     id,
                     authID,
                     uri,
                     desc,
                     sort,
                     createTime,
                     updateTime
                 )
                 VALUES (
                     14,
                     5,
                     '/logs/clear',
                     '[系统日志] 清除 7 天前日志接口',
                     100020,
                     63933638061,
                     63933904915
                 );

INSERT INTO uris (
                     id,
                     authID,
                     uri,
                     desc,
                     sort,
                     createTime,
                     updateTime
                 )
                 VALUES (
                     15,
                     3,
                     '/logout',
                     '注销登录接口',
                     30,
                     63933638061,
                     63933904983
                 );

INSERT INTO uris (
                     id,
                     authID,
                     uri,
                     desc,
                     sort,
                     createTime,
                     updateTime
                 )
                 VALUES (
                     16,
                     9,
                     '/auth/auth',
                     '[权限分组] 主接口',
                     403010,
                     63933638061,
                     63933904381
                 );

INSERT INTO uris (
                     id,
                     authID,
                     uri,
                     desc,
                     sort,
                     createTime,
                     updateTime
                 )
                 VALUES (
                     17,
                     6,
                     '/auth/user',
                     '[用户管理] 主接口',
                     400010,
                     63933638061,
                     63933904407
                 );

INSERT INTO uris (
                     id,
                     authID,
                     uri,
                     desc,
                     sort,
                     createTime,
                     updateTime
                 )
                 VALUES (
                     18,
                     5,
                     '/view/logs',
                     '[系统日志] 主页面视图',
                     100000,
                     63933640875,
                     63933904827
                 );

INSERT INTO uris (
                     id,
                     authID,
                     uri,
                     desc,
                     sort,
                     createTime,
                     updateTime
                 )
                 VALUES (
                     23,
                     3,
                     '/menu',
                     '管理后台左侧菜单数据接口',
                     20,
                     63933645834,
                     63933904968
                 );

INSERT INTO uris (
                     id,
                     authID,
                     uri,
                     desc,
                     sort,
                     createTime,
                     updateTime
                 )
                 VALUES (
                     30,
                     9,
                     '/view/auth/auth/edit',
                     '[权限分组] - [编辑分组] 子页面视图',
                     403030,
                     63933720099,
                     63933904753
                 );

INSERT INTO uris (
                     id,
                     authID,
                     uri,
                     desc,
                     sort,
                     createTime,
                     updateTime
                 )
                 VALUES (
                     31,
                     9,
                     '/view/auth/auth/add',
                     '[权限分组] - [添加分组] 子页面视图',
                     403020,
                     63933720099,
                     63933904731
                 );

INSERT INTO uris (
                     id,
                     authID,
                     uri,
                     desc,
                     sort,
                     createTime,
                     updateTime
                 )
                 VALUES (
                     32,
                     8,
                     '/view/auth/group/add',
                     '[权限分类] - [添加分类] 子页面视图',
                     402020,
                     63933805653,
                     63933904697
                 );

INSERT INTO uris (
                     id,
                     authID,
                     uri,
                     desc,
                     sort,
                     createTime,
                     updateTime
                 )
                 VALUES (
                     33,
                     8,
                     '/view/auth/group/edit',
                     '[权限分类] - [编辑分类] 子页面视图',
                     402030,
                     63933805653,
                     63933904710
                 );

INSERT INTO uris (
                     id,
                     authID,
                     uri,
                     desc,
                     sort,
                     createTime,
                     updateTime
                 )
                 VALUES (
                     34,
                     7,
                     '/view/auth/role/edit',
                     '[角色管理] - [编辑角色] 子页面视图',
                     401030,
                     63933809706,
                     63933904591
                 );

INSERT INTO uris (
                     id,
                     authID,
                     uri,
                     desc,
                     sort,
                     createTime,
                     updateTime
                 )
                 VALUES (
                     35,
                     7,
                     '/view/auth/role/add',
                     '[角色管理] - [添加角色] 子页面视图',
                     401020,
                     63933809706,
                     63933904571
                 );

INSERT INTO uris (
                     id,
                     authID,
                     uri,
                     desc,
                     sort,
                     createTime,
                     updateTime
                 )
                 VALUES (
                     36,
                     6,
                     '/view/auth/user/add',
                     '[用户管理] - [添加用户] 子页面视图',
                     400020,
                     63933814573,
                     63933904510
                 );

INSERT INTO uris (
                     id,
                     authID,
                     uri,
                     desc,
                     sort,
                     createTime,
                     updateTime
                 )
                 VALUES (
                     37,
                     6,
                     '/view/auth/user/edit',
                     '[用户管理] - [编辑用户] 子页面视图',
                     400030,
                     63933814573,
                     63933904534
                 );

INSERT INTO uris (
                     id,
                     authID,
                     uri,
                     desc,
                     sort,
                     createTime,
                     updateTime
                 )
                 VALUES (
                     38,
                     11,
                     '/option',
                     '[设置管理] 主接口',
                     200010,
                     63934828369,
                     63934949473
                 );

INSERT INTO uris (
                     id,
                     authID,
                     uri,
                     desc,
                     sort,
                     createTime,
                     updateTime
                 )
                 VALUES (
                     39,
                     11,
                     '/view/option',
                     '[设置管理] 主页面视图',
                     200000,
                     63934828369,
                     63934949414
                 );

INSERT INTO uris (
                     id,
                     authID,
                     uri,
                     desc,
                     sort,
                     createTime,
                     updateTime
                 )
                 VALUES (
                     40,
                     12,
                     '/view/option/menu/edit',
                     '[菜单管理] 编辑菜单视图',
                     201040,
                     63934829866,
                     63934949602
                 );

INSERT INTO uris (
                     id,
                     authID,
                     uri,
                     desc,
                     sort,
                     createTime,
                     updateTime
                 )
                 VALUES (
                     41,
                     12,
                     '/view/option/menu/add',
                     '[菜单管理] 添加菜单视图',
                     201030,
                     63934829866,
                     63934949575
                 );

INSERT INTO uris (
                     id,
                     authID,
                     uri,
                     desc,
                     sort,
                     createTime,
                     updateTime
                 )
                 VALUES (
                     42,
                     12,
                     '/view/option/menu',
                     '[菜单管理] 主页面视图',
                     201000,
                     63934829866,
                     63934949497
                 );

INSERT INTO uris (
                     id,
                     authID,
                     uri,
                     desc,
                     sort,
                     createTime,
                     updateTime
                 )
                 VALUES (
                     43,
                     12,
                     '/option/menu',
                     '[菜单管理] 主接口',
                     201010,
                     63934829866,
                     63934949520
                 );

INSERT INTO uris (
                     id,
                     authID,
                     uri,
                     desc,
                     sort,
                     createTime,
                     updateTime
                 )
                 VALUES (
                     44,
                     12,
                     '/view/option/menu/add/category',
                     '[菜单管理] 添加分类视图',
                     201020,
                     63934843214,
                     63934949555
                 );

INSERT INTO uris (
                     id,
                     authID,
                     uri,
                     desc,
                     sort,
                     createTime,
                     updateTime
                 )
                 VALUES (
                     45,
                     15,
                     '/trace/route',
                     '获取路由表数据',
                     500040,
                     63934856181,
                     63934949731
                 );

INSERT INTO uris (
                     id,
                     authID,
                     uri,
                     desc,
                     sort,
                     createTime,
                     updateTime
                 )
                 VALUES (
                     46,
                     15,
                     '/trace/auth',
                     '获取权限相关缓存数据',
                     500030,
                     63934856181,
                     63934949719
                 );

INSERT INTO uris (
                     id,
                     authID,
                     uri,
                     desc,
                     sort,
                     createTime,
                     updateTime
                 )
                 VALUES (
                     47,
                     15,
                     '/trace/session',
                     '获取完整的 Session 缓存数据',
                     500010,
                     63934856181,
                     63934949690
                 );

INSERT INTO uris (
                     id,
                     authID,
                     uri,
                     desc,
                     sort,
                     createTime,
                     updateTime
                 )
                 VALUES (
                     48,
                     15,
                     '/trace/option',
                     '获取完整的 Option 配置缓存数据',
                     500020,
                     63934856182,
                     63934949703
                 );

INSERT INTO uris (
                     id,
                     authID,
                     uri,
                     desc,
                     sort,
                     createTime,
                     updateTime
                 )
                 VALUES (
                     49,
                     15,
                     '/trace',
                     '获取全局缓存概览',
                     500000,
                     63934856182,
                     63934949671
                 );


COMMIT TRANSACTION;
PRAGMA foreign_keys = on;
