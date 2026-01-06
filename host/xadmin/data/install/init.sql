--
-- SQLiteStudio v3.4.17 生成的文件，周二 12月 30 16:29:05 2025
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


-- 表：user
CREATE TABLE user (
    id         INTEGER PRIMARY KEY AUTOINCREMENT,
    role       INTEGER,
    authLevel INTEGER DEFAULT (0),
    user       TEXT,
    salt       TEXT,
    pwd        TEXT,
    createTime INTEGER,
    updateTime INTEGER,
    isDelete   INTEGER
);

INSERT INTO user (
                     id,
                     role,
                     user,
                     salt,
                     pwd,
                     createTime,
                     updateTime,
                     isDelete
                 )
                 VALUES (
                     1,
                     1,
                     'admin',
                     'yT30uWu00001a1EiofyV4b9-_aHNO0aS',
                     '4FD681049123E6D9E48D534D7AA7DACEDE38C8860B14069647FF1E4446573521',
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
    createTime INTEGER,
    updateTime INTEGER,
    isDelete   INTEGER
);

INSERT INTO role (
                     id,
                     name,
                     desc,
                     authList,
                     createTime,
                     updateTime,
                     isDelete
                 )
                 VALUES (
                     1,
                     '超级管理员',
                     '超级管理员账户，拥有后台的全部权限',
                     '[1,3,2,4,5,6,7,8,9,10]',
                     63930031353,
                     63933905610,
                     0
                 );

INSERT INTO role (
                     id,
                     name,
                     desc,
                     authList,
                     createTime,
                     updateTime,
                     isDelete
                 )
                 VALUES (
                     2,
                     '管理员',
                     '拥有除了权限管理外的所有权限',
                     '[3,2,4]',
                     63930031443,
                     63933907648,
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
                          '权限管理',
                          '[权限管理] 菜单',
                          200000,
                          63933903052,
                          63933903052,
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
                     2,
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
                     6,
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
                     200000,
                     63933903342,
                     63933909352,
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
                     201000,
                     63933903366,
                     63933903979,
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
                     202000,
                     63933903387,
                     63933904075,
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
                     203000,
                     63933903428,
                     63933904081,
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
                     204000,
                     63933903456,
                     63933904085,
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
                     204000,
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
                     203000,
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
                     200000,
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
                     202000,
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
                     204010,
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
                     202010,
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
                     204020,
                     63933638061,
                     63933904782
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
                     201000,
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
                     200040,
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
                     201010,
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
                     203010,
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
                     200010,
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
                     19,
                     2,
                     '/view/services/edit',
                     '[服务管理] - [编辑服务] 子页面视图',
                     1030,
                     63933642911,
                     63933905179
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
                     20,
                     2,
                     '/view/services/add',
                     '[服务管理] - [添加服务] 子页面视图',
                     1020,
                     63933642911,
                     63933905165
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
                     21,
                     2,
                     '/services',
                     '[服务管理] 主接口',
                     1010,
                     63933642911,
                     63933905072
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
                     22,
                     2,
                     '/view/services',
                     '[服务管理] 主页面视图',
                     1000,
                     63933642911,
                     63933905012
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
                     24,
                     4,
                     '/tasks',
                     '[服务列表] - [任务管理] 主接口',
                     2010,
                     63933646916,
                     63933905391
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
                     25,
                     4,
                     '/view/tasks/edit',
                     '[服务列表] - [任务管理] - [编辑任务] 子页面视图',
                     2030,
                     63933646916,
                     63933905493
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
                     26,
                     4,
                     '/view/tasks/add',
                     '[服务列表] - [任务管理] - [添加任务] 子页面视图',
                     2020,
                     63933646916,
                     63933905480
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
                     27,
                     4,
                     '/view/services/tasks',
                     '[服务列表] - [任务管理] 主页面视图',
                     2000,
                     63933646916,
                     63933905237
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
                     28,
                     4,
                     '/view/tasks/logs',
                     '[服务列表] - [任务管理] - [查看日志] 子页面视图',
                     2040,
                     63933649752,
                     63933905517
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
                     29,
                     4,
                     '/task/logs',
                     '[服务列表] - [任务管理] - [查看日志] 接口',
                     2050,
                     63933649752,
                     63933905540
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
                     203030,
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
                     203020,
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
                     202020,
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
                     202030,
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
                     201030,
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
                     201020,
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
                     200020,
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
                     200030,
                     63933814573,
                     63933904534
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

INSERT INTO menu (id, parent, title, icon, type, openType, href, sort, visible, remark, createTime, updateTime, isDelete)
VALUES (1, 0, '主页', 'layui-icon layui-icon-home', 1, '_iframe', 'view/home', 0, 1, '系统主页', 63933640350, 63933640350, 0);

INSERT INTO menu (id, parent, title, icon, type, openType, href, sort, visible, remark, createTime, updateTime, isDelete)
VALUES (2, 0, '系统日志', 'layui-icon layui-icon-log', 1, '_component', 'view/logs', 100000, 1, '查看系统日志', 63933640350, 63933640350, 0);

INSERT INTO menu (id, parent, title, icon, type, openType, href, sort, visible, remark, createTime, updateTime, isDelete)
VALUES (3, 0, '权限管理', 'layui-icon layui-icon-auz', 0, NULL, NULL, 200000, 1, '权限管理目录', 63933640350, 63933640350, 0);

INSERT INTO menu (id, parent, title, icon, type, openType, href, sort, visible, remark, createTime, updateTime, isDelete)
VALUES (4, 3, '用户管理', 'layui-icon layui-icon-username', 1, '_component', 'view/auth/user', 200100, 1, '管理后台用户', 63933640350, 63933640350, 0);

INSERT INTO menu (id, parent, title, icon, type, openType, href, sort, visible, remark, createTime, updateTime, isDelete)
VALUES (5, 3, '角色管理', 'layui-icon layui-icon-user', 1, '_component', 'view/auth/role', 200200, 1, '管理用户角色', 63933640350, 63933640350, 0);

INSERT INTO menu (id, parent, title, icon, type, openType, href, sort, visible, remark, createTime, updateTime, isDelete)
VALUES (6, 3, '权限分类', 'layui-icon layui-icon-tabs', 1, '_component', 'view/auth/group', 200300, 1, '管理权限分类', 63933640350, 63933640350, 0);

INSERT INTO menu (id, parent, title, icon, type, openType, href, sort, visible, remark, createTime, updateTime, isDelete)
VALUES (7, 3, '权限分组', 'layui-icon layui-icon-template', 1, '_component', 'view/auth/auth', 200400, 1, '管理权限分组', 63933640350, 63933640350, 0);

INSERT INTO menu (id, parent, title, icon, type, openType, href, sort, visible, remark, createTime, updateTime, isDelete)
VALUES (8, 3, '接口管理', 'layui-icon layui-icon-website', 1, '_component', 'view/auth/uris', 200500, 1, '管理API接口', 63933640350, 63933640350, 0);

INSERT INTO menu (id, parent, title, icon, type, openType, href, sort, visible, remark, createTime, updateTime, isDelete)
VALUES (9, 3, '菜单管理', 'layui-icon layui-icon-spread-left', 1, '_component', 'view/auth/menu', 200600, 1, '管理系统菜单', 63933640350, 63933640350, 0);

INSERT INTO menu (id, parent, title, icon, type, openType, href, sort, visible, remark, createTime, updateTime, isDelete)
VALUES (10, 0, '全局配置', 'layui-icon layui-icon-set', 1, '_component', 'view/option?file=global', 150000, 1, '系统全局配置管理', 63933640350, 63933640350, 0);


COMMIT TRANSACTION;
PRAGMA foreign_keys = on;
